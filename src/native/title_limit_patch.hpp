#pragma once

#include "memory.hpp"
#include <safetyhook/mid_hook.hpp>
#include <array>

namespace point_zero::native::detail {
// The one selected five-byte COMISS/JBE title site. Uses public SafetyHook0.7
// APIs. Locators are discovered after enable but accepted only after independently
// verifying the fixed Windows x64 stub, exact callback and reconstructed branch
// trampoline/relay. No arbitrary read-back bytes become an ownership baseline.
// Before enable, only native bytes/public disabled metadata can be verified.
// Owner must retain this object BEFORE attempting enable, including on failure.
class TitleLimitPatch final {
public:
    TitleLimitPatch(MemoryView, std::uintptr_t, safetyhook::MidHookFn);
    TitleLimitPatch(const TitleLimitPatch&) = delete;
    TitleLimitPatch& operator=(const TitleLimitPatch&) = delete;
    bool before_intact() const noexcept;
    void enable();
    bool intact() const noexcept;
private:
    MemoryView memory_;
    std::uintptr_t target_{}, callback_{}, stub_{}, trampoline_{};
    std::array<std::byte, 5> patched_{};
    std::array<std::byte, 28> bridge_{};
    safetyhook::MidHook hook_;
    bool verified_{};
    void identify();
    bool backing_is(MemoryView) const noexcept;
};
} // namespace point_zero::native::detail
