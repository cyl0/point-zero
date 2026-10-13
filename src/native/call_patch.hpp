#pragma once

#include "memory.hpp"
#include <safetyhook/allocator.hpp>
#include <array>

namespace point_zero::native::detail {
// Redirect one E8 CALL through a leaf tail relay, preserving native unwind paths.
// Once exposed, the owner must retain this allocation for process lifetime.
class CallPatch final {
public:
    CallPatch(MemoryView, std::uintptr_t site, std::uintptr_t callee, std::uintptr_t destination);
    bool before_intact() const noexcept;
    bool intact() const noexcept;
    void enable();
    std::uintptr_t original() const noexcept { return original_; }
private:
    bool matches(std::span<const std::byte>) const noexcept;
    MemoryView memory_;
    std::uintptr_t site_, original_;
    safetyhook::Allocation relay_;
    std::array<std::byte, 5> before_{}, patched_{};
    std::array<std::byte, 14> bridge_{};
};
}
