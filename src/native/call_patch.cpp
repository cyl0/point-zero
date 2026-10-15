#include "call_patch.hpp"
#include <safetyhook/os.hpp>
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace point_zero::native::detail {
namespace {
void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
std::array<std::byte, 5> call(std::uintptr_t site, std::uintptr_t target) {
    require(site >= 0x10000 && site <= INTPTR_MAX - 5 && target >= 0x10000 && target <= INTPTR_MAX,
            "call hook address invalid");
    const auto delta = static_cast<std::int64_t>(target) - static_cast<std::int64_t>(site + 5);
    require(delta >= INT32_MIN && delta <= INT32_MAX, "call hook target out of range");
    std::array<std::byte, 5> result{std::byte{0xe8}};
    const auto displacement = static_cast<std::int32_t>(delta);
    std::memcpy(result.data() + 1, &displacement, sizeof(displacement));
    return result;
}
bool code_is(MemoryView memory, std::uintptr_t address, std::span<const std::byte> expected) noexcept {
    for (auto cursor = address; cursor < address + expected.size();) {
        MemoryRegion region{};
        if (!memory.query || !memory.query(memory.context, cursor, region) ||
            !contains(region.begin, region.size, cursor, 1) || !region.executable) return false;
        cursor = (std::min)(address + expected.size(), region.begin + region.size);
    }
    std::array<std::byte, 14> observed{};
    return read_bytes(memory, address, std::span(observed).first(expected.size())) &&
        std::equal(expected.begin(), expected.end(), observed.begin());
}
}
CallPatch::CallPatch(MemoryView memory, std::uintptr_t site,
                     std::uintptr_t destination) : memory_(memory), site_(site) {
    require(callable_range(memory, site, before_.size()) && read_bytes(memory, site, before_) &&
            before_[0] == std::byte{0xe8}, "call hook requires a direct CALL");
    std::int32_t displacement{};
    std::memcpy(&displacement, before_.data() + 1, sizeof(displacement));
    original_ = static_cast<std::uintptr_t>(site + 5 + displacement);
    require(callable_range(memory, original_), "call hook original target unavailable");
    require(destination >= 0x10000, "call hook destination missing");
    auto allocation = safetyhook::Allocator::global()->allocate_near(
        {reinterpret_cast<std::uint8_t*>(site)}, bridge_.size(), 0x7fff0000);
    require(allocation.has_value(), "call hook relay allocation failed");
    relay_ = std::move(*allocation);
    bridge_[0] = std::byte{0xff}; bridge_[1] = std::byte{0x25};
    std::memcpy(bridge_.data() + 6, &destination, sizeof(destination));
    std::memcpy(relay_.data(), bridge_.data(), bridge_.size());
    patched_ = call(site, relay_.address());
    require(before_intact(), "call hook instructions changed");
}
bool CallPatch::matches(std::span<const std::byte> bytes) const noexcept {
    const auto live = current_process_memory();
    return code_is(memory_, site_, bytes) && code_is(live, site_, bytes) &&
        code_is(memory_, relay_.address(), bridge_) && code_is(live, relay_.address(), bridge_);
}
bool CallPatch::before_intact() const noexcept { return matches(before_); }
bool CallPatch::intact() const noexcept { return matches(patched_); }
void CallPatch::enable() {
    require(before_intact(), "call hook changed before enable");
    require(FlushInstructionCache(GetCurrentProcess(), relay_.data(), bridge_.size()) != FALSE,
            "call hook relay cache flush failed");
    auto* const target = reinterpret_cast<std::uint8_t*>(site_);
    bool written{};
    // Retry execution at the same CALL after its page becomes executable again.
    safetyhook::trap_threads(target, target, before_.size(), [&] {
        if (std::memcmp(target, before_.data(), before_.size()) == 0) {
            std::memcpy(target, patched_.data(), patched_.size());
            written = FlushInstructionCache(GetCurrentProcess(), target, patched_.size()) != FALSE;
        }
    });
    require(written && intact(), "enabled call hook provenance mismatch");
}
}
