#pragma once

#include "memory.hpp"
#include <safetyhook/inline_hook.hpp>
#include <vector>

namespace point_zero::native::detail {
// Exposed patches are retained for the lifetime of the process.
class InlinePatch final {
public:
    InlinePatch(MemoryView, std::uintptr_t target, std::span<const std::byte> body,
                std::uintptr_t destination);
    InlinePatch(const InlinePatch&) = delete;
    InlinePatch& operator=(const InlinePatch&) = delete;
    bool before_intact() const noexcept;
    bool intact() const noexcept;
    void enable();
    std::uintptr_t original() const noexcept { return trampoline_; }
private:
    MemoryView memory_;
    std::uintptr_t target_, trampoline_{};
    std::vector<std::byte> before_, patched_;
    safetyhook::InlineHook hook_;
    bool body_is(std::span<const std::byte>) const noexcept;
};
}
