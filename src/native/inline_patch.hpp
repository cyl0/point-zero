#pragma once

#include "memory.hpp"
#include <safetyhook/inline_hook.hpp>
#include <vector>

namespace point_zero::native::detail {
// Shared mechanics for the catalogue and surface owners. They retain this
// object forever BEFORE attempting enable; destroying an exposed patch is not
// supported. There is no callback registry or independent lifetime policy here.
// Only the complete instructions displaced by the entry jump are retained.
// Unpatched function bytes are not part of patch ownership.
class InlinePatch final {
public:
    InlinePatch(MemoryView, std::uintptr_t target, std::span<const std::byte> body,
                std::uintptr_t destination);
    InlinePatch(const InlinePatch&) = delete;
    InlinePatch& operator=(const InlinePatch&) = delete;
    [[nodiscard]] bool before_intact() const noexcept;
    [[nodiscard]] bool intact() const noexcept;
    void enable();
    [[nodiscard]] std::uintptr_t original() const noexcept { return trampoline_; }
private:
    MemoryView memory_;
    std::uintptr_t target_, trampoline_{};
    std::vector<std::byte> before_, patched_, bridge_;
    std::size_t stolen_{};
    safetyhook::InlineHook hook_;
    bool body_is(std::span<const std::byte>) const noexcept;
    bool bridge_is() const noexcept;
};
} // namespace point_zero::native::detail
