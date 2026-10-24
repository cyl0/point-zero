#pragma once

#include "memory.hpp"
#include <safetyhook/mid_hook.hpp>
#include <array>
#include <vector>

namespace point_zero::native::detail {
// Finite SafetyHook0.7 Windows x64 captures: HUD,
// result, notes, Step Up, category-summary binding, category Back, boot text
// and the guide's two pre-call MOVs. before is the already-admitted native slice starting at
// target (possibly larger than the stolen 5/6/8 bytes); all of it is checked.
// Before enable, inspect only native code and public disabled metadata. After
// enable, locate private storage through the entry and independently verify it.
// Owner must retain this object BEFORE attempting enable, including on failure.
// These checks are admission/ownership checks, never capture-callback hot paths.
//
// Context.rsp is the native RSP at capture, before displaced instructions run.
// The pinned stub repairs its saved RSP by +16 before invoking the callback.
// SafetyHook supplies no unwind metadata for its private stub/trampoline:
// exceptions must not escape callbacks or cross relocated calls into that
// storage. Native unwind coverage applies again after the jump back to native
// code; owning functions' unwind data does not cover these private allocations.
class PlayedMidPatch final {
public:
    PlayedMidPatch(MemoryView, std::uintptr_t target, std::span<const std::byte> before,
                   safetyhook::MidHookFn, const GameBindings& = october_bindings);
    PlayedMidPatch(const PlayedMidPatch&) = delete;
    PlayedMidPatch& operator=(const PlayedMidPatch&) = delete;
    bool before_intact() const noexcept;
    void enable();
    bool intact() const noexcept;
private:
    MemoryView memory_;
    std::uintptr_t target_{}, callback_{}, call_target_{}, stub_{}, trampoline_{};
    std::vector<std::byte> before_, patched_;
    std::array<std::byte, 28> bridge_{};
    std::size_t stolen_{};
    safetyhook::MidHook hook_;
    bool verified_{};
    void identify();
    bool metadata_is() const noexcept;
    bool backing_is(MemoryView) const noexcept;
};
} // namespace point_zero::native::detail
