#include "inline_patch.hpp"

#include <windows.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>

namespace point_zero::native::detail {
namespace {
void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
}
InlinePatch::InlinePatch(MemoryView memory, std::uintptr_t target,
                         std::span<const std::byte> body, std::uintptr_t destination)
    : memory_(memory), target_(target) {
    require(callable_range(memory, target, body.size()) && callable_range(memory, destination),
            "inline hook code unavailable");
    auto hook = safetyhook::InlineHook::create(reinterpret_cast<void*>(target),
        reinterpret_cast<void*>(destination), safetyhook::InlineHook::StartDisabled);
    require(hook.has_value(), "inline hook creation failed");
    hook_ = std::move(*hook);
    const auto& saved = hook_.original_bytes();
    require(saved.size() <= body.size() &&
            std::memcmp(saved.data(), body.data(), saved.size()) == 0,
            "inline instructions changed during preparation");
    before_.assign(body.begin(), body.begin() + saved.size());
    patched_.resize(saved.size());
    trampoline_ = hook_.trampoline().address();
    require(before_intact(), "inline hook changed during preparation");
}
bool InlinePatch::body_is(std::span<const std::byte> expected) const noexcept {
    std::array<std::byte, 32> actual{};
    return expected.size() <= actual.size() && callable_range(memory_, target_, expected.size()) &&
        read_bytes(memory_, target_, std::span(actual).first(expected.size())) &&
        std::equal(expected.begin(), expected.end(), actual.begin());
}
bool InlinePatch::before_intact() const noexcept {
    return body_is(before_) && callable_range(memory_, trampoline_, hook_.trampoline().size());
}
bool InlinePatch::intact() const noexcept {
    return hook_.enabled() && body_is(patched_);
}
void InlinePatch::enable() {
    require(before_intact(), "inline hook changed before enable");
    require(FlushInstructionCache(GetCurrentProcess(), hook_.trampoline().data(),
        hook_.trampoline().size()) != FALSE, "inline trampoline cache flush failed");
    require(hook_.enable().has_value(), "inline hook enable failed");
    require(FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(target_),
        before_.size()) != FALSE, "inline target cache flush failed");
    require(read_bytes(memory_, target_, patched_), "inline patch read failed");
}
}
