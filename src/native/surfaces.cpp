#include "surfaces.hpp"
#include "build_bindings.hpp"
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
std::uintptr_t game_code(const AdapterContext& context, std::uint32_t rva) {
    const auto game = context.modules.game.range;
    require(contains(game.base, game.size, game.base + rva, 1) &&
            callable_range(context.memory, game.base + rva), "surface native call unavailable");
    return game.base + rva;
}
std::uintptr_t game_data(const AdapterContext& context, std::uint32_t rva, std::size_t size) {
    const auto game = context.modules.game.range;
    require(readable_range(context.memory, game.base + rva, size, RangeUse::image_read, game),
            "surface native data unavailable");
    return game.base + rva;
}
CupNativeCalls primary_calls(const AdapterContext& context) {
    const auto& b = game_bindings(context.modules.game.range).primary_contract;
    CupNativeCalls calls;
    calls.style = reinterpret_cast<decltype(calls.style)>(game_code(context, b.style_rva));
    calls.clock = reinterpret_cast<decltype(calls.clock)>(game_code(context, b.clock_rva));
    calls.rate = reinterpret_cast<decltype(calls.rate)>(game_code(context, b.rate_rva));
    return calls;
}
ConfirmationNativeCalls confirmation_calls(const AdapterContext& context) {
    const auto& b = game_bindings(context.modules.game.range).confirmation_contract;
    ConfirmationNativeCalls calls;
    calls.style = reinterpret_cast<decltype(calls.style)>(game_code(context, b.style_rva));
    calls.logical_count = reinterpret_cast<decltype(calls.logical_count)>(game_code(context, b.logical_count_rva));
    calls.sole_player = reinterpret_cast<decltype(calls.sole_player)>(game_code(context, b.sole_player_rva));
    calls.raw_mode = reinterpret_cast<decltype(calls.raw_mode)>(game_code(context, b.raw_mode_rva));
    calls.published_music = game_data(context, b.published_music_rva, sizeof(std::uintptr_t));
    calls.published_difficulties = game_data(context, b.published_difficulties_rva, 2 * sizeof(int));
    calls.name_table = game_data(context, b.name_table_rva, 65 * sizeof(std::uintptr_t));
    for (std::size_t i = 0; i < 2; ++i) {
        calls.setter_callers[i] = game_code(context, b.setter_callers[i]);
        calls.named_callers[i] = game_code(context, b.named_callers[i]);
    }
    calls.builder_caller = game_code(context, b.builder_caller_rva);
    return calls;
}
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
            cup_ = std::make_unique<CupRenderer>(context_, admit_cup_calls(memory, modules.afp, modules.utils),
                                                  primary_calls(context_));
        });
        attempt(context_, "confirmation rendering", [&] {
            confirmation_ = std::make_unique<ConfirmationRenderer>(context_,
                admit_cup_calls(memory, modules.afp, modules.utils), confirmation_calls(context_));
        });
        attempt(context_, "played rendering", [&] {
            played_ = std::make_unique<PlayedRenderer>(context_, admit_played_calls(memory, modules));
        });
        attempt(context_, "Dan rendering", [&] {
            dan_ = std::make_unique<DanRenderer>(context_);
        });
        attempt(context_, "named surface callbacks", [&] {
            played_hooks_ = std::make_unique<PlayedHooks>(memory,
                admit_played_sites(memory, modules.game), admit_stepup_sites(memory, modules.game),
                admit_dan_sites(memory, modules.game), this, played_callbacks());
        });
        attempt(context_, "Assistant rendering", [&] {
            assistant_ = std::make_unique<AssistantRenderer>(context_, stock_bitmap_);
            const auto& b = game_bindings(modules.game.range).assistant_header_contract;
            const auto base = modules.game.range.base;
            const AssistantHeaderSites sites{base + b.update_rva, base + b.header_rva,
                base + b.style_rva, base + b.player_rva};
            assistant_hook_ = std::make_unique<AssistantHeaderHook>(memory, modules.game.range,
                sites, assistant_.get(), AssistantRenderer::callbacks());
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
    if (played_ && played_hooks_) played_ready_ = attempt(context_, "played artwork", [&] {
        require(slots[2] && slots[4] && slots[5] && played_->preload(slots[2], slots[4], slots[5]),
                "package unavailable");
    });
    if (confirmation_ && played_hooks_) confirmation_ready_ = attempt(context_, "confirmation artwork", [&] {
        require(slots[3] && confirmation_->preload(slots[3]), "package unavailable");
    });
    if (assistant_hook_) assistant_ready_ = attempt(context_, "Assistant artwork", [&] {
        require(slots[6] && assistant_->preload(slots[6]), "package unavailable");
    });
    if (dan_ && played_hooks_) dan_ready_ = attempt(context_, "Dan artwork", [&] {
        require(slots[0] && slots[1] && dan_->preload(slots[0], slots[1]), "package unavailable");
    });
    preparing();
    const auto arm = [this](std::size_t index, auto& hook, const char* feature) {
        arms_[index] = ArmState::attempted;
        const bool armed = attempt(context_, feature, [&] { hook->arm(); });
        if (armed) arms_[index] = ArmState::armed;
        preparing();
        return armed;
    };
    if ((cup_ready_ || played_ready_ || confirmation_ready_ || assistant_ready_ || dan_ready_) &&
        !arm(0, surface_hooks_, "bitmap rendering"))
        cup_ready_ = played_ready_ = confirmation_ready_ = assistant_ready_ = dan_ready_ = false;
    if (list_ready && arm(1, list_hook_, "list rendering") && title_hook_)
        arm(2, title_hook_, "list title adjustment");
    if ((played_ready_ || confirmation_ready_ || dan_ready_) && !arm(3, played_hooks_, "named surface callbacks"))
        played_ready_ = confirmation_ready_ = dan_ready_ = false;
    if (assistant_ready_ && !arm(4, assistant_hook_, "Assistant rendering")) assistant_ready_ = false;
}

bool NativeSurfaces::available() const noexcept {
    return cup_ready_ || arms_[1] == ArmState::armed || played_ready_ || confirmation_ready_ || assistant_ready_ || dan_ready_;
}
bool NativeSurfaces::intact() const noexcept {
    return (arms_[0] == ArmState::unattempted || (arms_[0] == ArmState::armed && surface_hooks_->intact())) &&
           (arms_[1] == ArmState::unattempted || (arms_[1] == ArmState::armed && list_hook_->intact())) &&
           (arms_[2] == ArmState::unattempted || (arms_[2] == ArmState::armed && title_hook_->intact())) &&
           (arms_[3] == ArmState::unattempted || (arms_[3] == ArmState::armed && played_hooks_->intact())) &&
           (arms_[4] == ArmState::unattempted || (arms_[4] == ArmState::armed && assistant_hook_->intact()));
}
bool NativeSurfaces::rollback() noexcept {
    retire_callbacks();
    return context_.state.view().phase == FeaturePhase::preparing && intact();
}
void NativeSurfaces::retire_callbacks() noexcept {
    if (retired_) return;
    retired_ = true;
    if (assistant_hook_) assistant_hook_->retire_callbacks();
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
            return (self.dan_ready_ && self.dan_->bitmap(mc, name, mode, result, stock)) ||
                   (self.confirmation_ready_ && self.confirmation_->bitmap(mc, name, mode, result, stock)) ||
                   (self.played_ready_ && self.played_->bitmap(mc, name, mode, result, stock)) ||
                   (self.cup_ready_ && self.cup_->bitmap(mc, name, mode, result, stock));
        },
        [](void* opaque) noexcept {
            auto& self = *static_cast<NativeSurfaces*>(opaque);
            if (self.cup_ready_) self.cup_->begin_set();
        },
        [](void* opaque, std::uintptr_t owner, std::uintptr_t music) noexcept {
            auto& self = *static_cast<NativeSurfaces*>(opaque);
            if (self.cup_ready_) self.cup_->bind(owner, music);
        },
        [](void* opaque) noexcept {
            auto& self = *static_cast<NativeSurfaces*>(opaque);
            if (self.confirmation_ready_) self.confirmation_->begin_set();
        },
        [](void* opaque, std::uintptr_t owner, std::uintptr_t music, std::uintptr_t caller) noexcept {
            auto& self = *static_cast<NativeSurfaces*>(opaque);
            if (self.confirmation_ready_) self.confirmation_->bind(owner, music, caller);
        },
        [](void* opaque, std::uintptr_t owner, std::uintptr_t caller) noexcept {
            auto& self = *static_cast<NativeSurfaces*>(opaque);
            if (self.confirmation_ready_) self.confirmation_->begin(owner, caller);
        },
        [](void* opaque) noexcept {
            auto& self = *static_cast<NativeSurfaces*>(opaque);
            if (self.confirmation_ready_) self.confirmation_->end();
        }
    };
}
PlayedHooks::Callbacks NativeSurfaces::played_callbacks() noexcept {
    return {
        [](void* opaque, const PlayedCapture& capture) noexcept {
            auto& self = *static_cast<NativeSurfaces*>(opaque);
            if (self.played_ready_) self.played_->capture(capture, self.stock_bitmap_);
        },
        [](void* opaque, const PlayedCapture& capture, std::uintptr_t wrapper, const char* name,
           std::uintptr_t caller) noexcept {
            auto& self = *static_cast<NativeSurfaces*>(opaque);
            if (self.confirmation_ready_) self.confirmation_->begin_named(wrapper, name, caller);
            if (self.played_ready_) self.played_->begin(capture, wrapper, name);
        },
        [](void* opaque) noexcept {
            auto& self = *static_cast<NativeSurfaces*>(opaque);
            if (self.dan_ready_) self.dan_->end();
            if (self.played_ready_) self.played_->end();
            if (self.confirmation_ready_) self.confirmation_->end_named();
        },
        [](void* opaque, std::uintptr_t wrapper) noexcept {
            auto& self = *static_cast<NativeSurfaces*>(opaque);
            if (self.played_ready_) self.played_->update_notes(wrapper);
        },
        [](void* opaque, const DanCapture& capture, std::uintptr_t wrapper, const char* name) noexcept {
            auto& self = *static_cast<NativeSurfaces*>(opaque);
            if (self.dan_ready_) self.dan_->begin(capture, wrapper, name);
        }
    };
}
} // namespace point_zero::native
