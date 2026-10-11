#include "surfaces.hpp"
#include "cup_contract.hpp"
#include "surface_contract.hpp"
#include "list_contract.hpp"
#include "list_hook_contract.hpp"
#include "list_title_contract.hpp"
#include "played_calls_contract.hpp"

#include <windows.h>
#include <stdexcept>

namespace point_zero::native {
namespace {
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
} // namespace

NativeSurfaces::NativeSurfaces(const AdapterContext& context) noexcept : context_(context) {}
NativeSurfaces::~NativeSurfaces() noexcept { retire_callbacks(); }

void NativeSurfaces::preparing() const {
    require(!retired_ && thread_ == GetCurrentThreadId() &&
            context_.state.view().phase == FeaturePhase::preparing,
            "surface preparation stopped or left its admitted native thread");
}
std::span<const std::string_view> NativeSurfaces::asset_families() noexcept { return families; }

void NativeSurfaces::prepare() {
    require(!prepare_attempted_, "surface preparation already attempted");
    prepare_attempted_ = true;
    thread_ = GetCurrentThreadId();
    preparing();
    const auto& modules = context_.modules;
    const auto memory = context_.memory;
    // Finish every finite contract before constructing any native hook. These
    // are initializer reads, not render-time checks or whole-module hash vetoes.
    const auto cup_calls = admit_cup_calls(memory, modules.afp, modules.utils);
    const auto list_calls = admit_list_calls(memory, modules);
    const auto played_calls = admit_played_calls(memory, modules, cup_calls);
    const auto surface_sites = admit_surface_sites(memory, modules.game, modules.afp);
    const auto list_site = admit_list_hook_site(memory, modules.game);
    const auto title_site = admit_list_title_site(memory, modules.game);
    const auto played_sites = admit_played_sites(memory, modules.game);
    const auto stepup_sites = admit_stepup_sites(memory, modules.game);
    preparing();
    cup_ = std::make_unique<CupRenderer>(context_, cup_calls);
    list_ = std::make_unique<ListRenderer>(context_, list_calls);
    played_ = std::make_unique<PlayedRenderer>(context_, played_calls);
    surface_hooks_ = std::make_unique<SurfaceHooks>(memory, surface_sites, this, bitmap_callbacks());
    stock_bitmap_ = surface_hooks_->stock_bitmap();
    require(stock_bitmap_ != nullptr, "surface stock bitmap continuation unavailable");
    list_hook_ = std::make_unique<ListHook>(memory, list_site, list_.get(), ListRenderer::callback());
    title_hook_ = std::make_unique<ListTitleHook>(memory, title_site, list_.get(), ListRenderer::title_callback());
    played_hooks_ = std::make_unique<PlayedHooks>(memory, played_sites, stepup_sites, this, played_callbacks());
    preparing();
    prepared_ = true;
}

void NativeSurfaces::begin_activation() {
    require(!activation_attempted_, "surface activation already attempted");
    activation_attempted_ = true;
    preparing();
    require(prepared_, "surface contracts and hooks have not been prepared");
}
void NativeSurfaces::activate(std::span<const LoadedAsset> assets) {
    begin_activation();
    std::array<std::uintptr_t, families.size()> slots{};
    for (const auto& asset : assets) for (std::size_t i = 0; i < families.size(); ++i) {
        if (asset.family != families[i]) continue;
        require(!slots[i] && asset.slot, "surface package missing a slot or duplicated");
        slots[i] = asset.slot;
    }
    for (std::size_t i = 0; i < slots.size(); ++i) {
        require(slots[i] != 0, "required surface package missing");
        for (std::size_t j = 0; j < i; ++j)
            require(slots[i] != slots[j], "surface packages alias the same native slot");
    }
    activate_slots(slots);
}
void NativeSurfaces::activate(const AssetSlots& slots) {
    begin_activation();
    activate_slots(slots);
}
void NativeSurfaces::activate_slots(const AssetSlots& slots) {
    require(cup_->preload(slots[0]), "cup artwork preparation failed");
    preparing();
    require(list_->preload(slots[1]), "list artwork preparation failed");
    preparing();
    require(played_->preload(slots[2]), "played artwork preparation failed");
    preparing();
    const auto arm = [this](std::size_t index, auto& hook) {
        arms_[index] = ArmState::attempted;
        hook->arm();
        arms_[index] = ArmState::armed;
        preparing();
    };
    arm(0, surface_hooks_);
    arm(1, list_hook_);
    arm(2, title_hook_);
    arm(3, played_hooks_);
    // Publication belongs to the complete native owner, after browsing/filter
    // initialization too. A surface-only success cannot enable custom values.
}

bool NativeSurfaces::intact() const noexcept {
    return (arms_[0] == ArmState::unattempted || (arms_[0] == ArmState::armed && surface_hooks_->intact())) &&
           (arms_[1] == ArmState::unattempted || (arms_[1] == ArmState::armed && list_hook_->intact())) &&
           (arms_[2] == ArmState::unattempted || (arms_[2] == ArmState::armed && title_hook_->intact())) &&
           (arms_[3] == ArmState::unattempted || (arms_[3] == ArmState::armed && played_hooks_->intact()));
}
bool NativeSurfaces::rollback() noexcept {
    retire_callbacks();
    // Successful forwarding patches may remain, but only before publication,
    // while no renderer has attached a package. A failed enable is ambiguous
    // even if the hook internally closed callbacks; retain its backing/assets.
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
            static_cast<NativeSurfaces*>(opaque)->cup_->begin(owner);
        },
        [](void* opaque) noexcept { static_cast<NativeSurfaces*>(opaque)->cup_->end(); },
        [](void* opaque, int mc, const char* name, int mode, int& result, SurfaceHooks::Bitmap stock) noexcept {
            auto& self = *static_cast<NativeSurfaces*>(opaque);
            // Both renderers retain their own precise native owner/name scope.
            // A handled result includes stock restoration or partial mutation;
            // never pass that call to the other renderer or forward it twice.
            return self.played_->bitmap(mc, name, mode, result, stock) ||
                   self.cup_->bitmap(mc, name, mode, result, stock);
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
