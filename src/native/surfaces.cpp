#include "surfaces.hpp"
#include "cup_contract.hpp"
#include "surface_contract.hpp"
#include "list_contract.hpp"
#include "list_hook_contract.hpp"
#include "list_title_contract.hpp"
#include "played_calls_contract.hpp"

#include <windows.h>
#include <stdexcept>
#include <string>

namespace point_zero::native {
namespace {
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
template<class Operation>
bool attempt(const AdapterContext& context, const char* feature, Operation operation) {
    try { operation(); return true; }
    catch (const std::exception& error) {
        if (context.host.log) {
            const auto message = std::string(feature) + " unavailable: " + error.what();
            context.host.log(context.host.context, PZ_LOG_WARNING,
                {reinterpret_cast<const std::uint8_t*>(message.data()), message.size()});
        }
        return false;
    }
}
} // namespace

NativeSurfaces::NativeSurfaces(const AdapterContext& context) noexcept : context_(context) {}
NativeSurfaces::~NativeSurfaces() noexcept { retire_callbacks(); }

void NativeSurfaces::preparing() const {
    require(!retired_ && thread_ == GetCurrentThreadId() &&
            context_.state.view().phase == FeaturePhase::preparing,
            "surface preparation stopped or left its native thread");
}
std::span<const std::string_view> NativeSurfaces::asset_families() noexcept { return families; }

void NativeSurfaces::prepare() {
    require(!prepare_attempted_, "surface preparation already attempted");
    prepare_attempted_ = true;
    thread_ = GetCurrentThreadId();
    preparing();
    const auto& modules = context_.modules;
    const auto memory = context_.memory;
    attempt(context_, "bitmap rendering", [&] {
        surface_hooks_ = std::make_unique<SurfaceHooks>(memory,
            admit_surface_sites(memory, modules.game, modules.afp), this, bitmap_callbacks());
        stock_bitmap_ = surface_hooks_->stock_bitmap();
        require(stock_bitmap_ != nullptr, "bitmap continuation unavailable");
    });
    if (surface_hooks_) {
        attempt(context_, "cup rendering", [&] {
            cup_ = std::make_unique<CupRenderer>(context_, admit_cup_calls(memory, modules.afp, modules.utils));
        });
        attempt(context_, "played rendering", [&] {
            played_ = std::make_unique<PlayedRenderer>(context_, admit_played_calls(memory, modules));
            played_hooks_ = std::make_unique<PlayedHooks>(memory,
                admit_played_sites(memory, modules.game), admit_stepup_sites(memory, modules.game),
                this, played_callbacks());
        });
    }
    attempt(context_, "list rendering", [&] {
        list_ = std::make_unique<ListRenderer>(context_, admit_list_calls(memory, modules));
        list_hook_ = std::make_unique<ListHook>(memory, admit_list_hook_site(memory, modules.game),
            list_.get(), ListRenderer::callback());
    });
    if (list_hook_) attempt(context_, "list title adjustment", [&] {
        title_hook_ = std::make_unique<ListTitleHook>(memory, admit_list_title_site(memory, modules.game),
            list_.get(), ListRenderer::title_callback());
    });
    preparing();
    prepared_ = true;
}

void NativeSurfaces::begin_activation() {
    require(!activation_attempted_, "surface activation already attempted");
    activation_attempted_ = true;
    preparing();
    require(prepared_, "surface hooks have not been prepared");
}
void NativeSurfaces::activate(std::span<const LoadedAsset> assets) {
    AssetSlots slots{};
    for (const auto& asset : assets) for (std::size_t i = 0; i < families.size(); ++i) {
        if (asset.family != families[i]) continue;
        require(!slots[i] && asset.slot, "surface package missing a slot or duplicated");
        slots[i] = asset.slot;
    }
    for (std::size_t i = 0; i < slots.size(); ++i) for (std::size_t j = 0; j < i; ++j)
        require(!slots[i] || slots[i] != slots[j], "surface packages alias the same native slot");
    activate(slots);
}
void NativeSurfaces::activate(const AssetSlots& slots) {
    begin_activation();
    activate_slots(slots);
}
void NativeSurfaces::activate_slots(const AssetSlots& slots) {
    if (cup_) cup_ready_ = attempt(context_, "cup artwork", [&] {
        require(slots[0] && cup_->preload(slots[0]), "package unavailable");
    });
    bool list_ready{};
    if (list_hook_) list_ready = attempt(context_, "list artwork", [&] {
        require(slots[1] && list_->preload(slots[1]), "package unavailable");
    });
    if (played_hooks_) played_ready_ = attempt(context_, "played artwork", [&] {
        require(slots[2] && played_->preload(slots[2]), "package unavailable");
    });
    preparing();
    const auto arm = [this](std::size_t index, auto& hook, const char* feature) {
        arms_[index] = ArmState::attempted;
        const bool armed = attempt(context_, feature, [&] { hook->arm(); });
        if (armed) arms_[index] = ArmState::armed;
        preparing();
        return armed;
    };
    if ((cup_ready_ || played_ready_) && !arm(0, surface_hooks_, "bitmap rendering"))
        cup_ready_ = played_ready_ = false;
    if (list_ready && arm(1, list_hook_, "list rendering") && title_hook_)
        arm(2, title_hook_, "list title adjustment");
    if (played_ready_ && !arm(3, played_hooks_, "played rendering")) played_ready_ = false;
}

bool NativeSurfaces::available() const noexcept {
    return cup_ready_ || arms_[1] == ArmState::armed || played_ready_;
}
bool NativeSurfaces::intact() const noexcept {
    return (arms_[0] == ArmState::unattempted || (arms_[0] == ArmState::armed && surface_hooks_->intact())) &&
           (arms_[1] == ArmState::unattempted || (arms_[1] == ArmState::armed && list_hook_->intact())) &&
           (arms_[2] == ArmState::unattempted || (arms_[2] == ArmState::armed && title_hook_->intact())) &&
           (arms_[3] == ArmState::unattempted || (arms_[3] == ArmState::armed && played_hooks_->intact()));
}
bool NativeSurfaces::rollback() noexcept {
    retire_callbacks();
    return context_.state.view().phase == FeaturePhase::preparing && intact();
}
void NativeSurfaces::retire_callbacks() noexcept {
    if (retired_) return;
    retired_ = true;
    if (played_hooks_) played_hooks_->retire_callbacks();
    if (title_hook_) title_hook_->retire_callbacks();
    if (list_hook_) list_hook_->retire_callback();
    if (surface_hooks_) surface_hooks_->retire_callbacks();
}

SurfaceHooks::Callbacks NativeSurfaces::bitmap_callbacks() noexcept {
    return {
        [](void* opaque, std::uintptr_t owner) noexcept {
            auto& self = *static_cast<NativeSurfaces*>(opaque);
            if (self.cup_ready_) self.cup_->begin(owner);
        },
        [](void* opaque) noexcept {
            auto& self = *static_cast<NativeSurfaces*>(opaque);
            if (self.cup_ready_) self.cup_->end();
        },
        [](void* opaque, int mc, const char* name, int mode, int& result, SurfaceHooks::Bitmap stock) noexcept {
            auto& self = *static_cast<NativeSurfaces*>(opaque);
            return (self.played_ready_ && self.played_->bitmap(mc, name, mode, result, stock)) ||
                   (self.cup_ready_ && self.cup_->bitmap(mc, name, mode, result, stock));
        }
    };
}
PlayedHooks::Callbacks NativeSurfaces::played_callbacks() noexcept {
    return {
        [](void* opaque, const PlayedCapture& capture) noexcept {
            auto& self = *static_cast<NativeSurfaces*>(opaque);
            self.played_->capture(capture, self.stock_bitmap_);
        },
        [](void* opaque, const PlayedCapture& capture, std::uintptr_t wrapper, const char* name) noexcept {
            static_cast<NativeSurfaces*>(opaque)->played_->begin(capture, wrapper, name);
        },
        [](void* opaque) noexcept { static_cast<NativeSurfaces*>(opaque)->played_->end(); },
        [](void* opaque, std::uintptr_t wrapper) noexcept {
            static_cast<NativeSurfaces*>(opaque)->played_->update_notes(wrapper);
        }
    };
}
} // namespace point_zero::native
