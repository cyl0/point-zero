#include "inline_patch.hpp"

#include <windows.h>
#include <Zydis/Zydis.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace point_zero::native::detail {
namespace {
static_assert(sizeof(std::uintptr_t) == 8 && std::endian::native == std::endian::little);
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
bool code_is(MemoryView memory, std::uintptr_t address,
             std::span<const std::byte> expected, bool private_code = false) noexcept {
    if (!memory.read || !memory.query || address < 0x10000 || expected.empty() ||
        expected.size() > UINTPTR_MAX - address) return false;
    const auto end = address + expected.size();
    for (auto cursor = address; cursor < end;) {
        MemoryRegion region{};
        if (!memory.query(memory.context, cursor, region) ||
            !contains(region.begin, region.size, cursor, 1) ||
            !region.committed || !region.readable || !region.executable || region.guarded ||
            (private_code && region.kind != MemoryKind::private_memory)) return false;
        cursor = (std::min)(end, region.begin + region.size);
    }
    std::array<std::byte, 256> buffer{};
    for (std::size_t offset = 0; offset < expected.size();) {
        const auto count = (std::min)(buffer.size(), expected.size() - offset);
        auto part = std::span(buffer).first(count);
        if (!read_bytes(memory, address + offset, part) ||
            !std::equal(part.begin(), part.end(), expected.begin() + offset)) return false;
        offset += count;
    }
    return true;
}
std::size_t stolen_size(std::span<const std::byte> body) {
    ZydisDecoder decoder{};
    require(ZYAN_SUCCESS(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64,
                                        ZYDIS_STACK_WIDTH_64)), "inline decoder init failed");
    std::size_t size{};
    while (size < 5) {
        ZydisDecodedInstruction instruction{};
        require(size < body.size() &&
                ZYAN_SUCCESS(ZydisDecoderDecodeInstruction(&decoder, nullptr,
                    body.data() + size, body.size() - size, &instruction)) &&
                instruction.length && !(instruction.attributes & ZYDIS_ATTRIB_IS_RELATIVE),
                "unsupported non-relative inline hook prologue");
        size += instruction.length;
    }
    return size;
}
template<class T> void put(std::vector<std::byte>& bytes, std::size_t at, T value) {
    std::memcpy(bytes.data() + at, &value, sizeof(value));
}
void jump(std::vector<std::byte>& bytes, std::size_t at,
          std::uintptr_t source, std::uintptr_t destination) {
    const auto delta = static_cast<std::int64_t>(destination) -
                       static_cast<std::int64_t>(source) - 5;
    require(delta >= (std::numeric_limits<std::int32_t>::min)() &&
            delta <= (std::numeric_limits<std::int32_t>::max)(), "inline jump out of range");
    bytes[at] = std::byte{0xe9};
    put(bytes, at + 1, static_cast<std::int32_t>(delta));
}
} // namespace

InlinePatch::InlinePatch(MemoryView memory, std::uintptr_t target,
                         std::span<const std::byte> body, std::uintptr_t destination)
    : memory_(memory), target_(target), before_(body.begin(), body.end()) {
    stolen_ = stolen_size(before_);
    before_.resize(stolen_);
    require(destination >= 0x10000 && body_is(before_), "inline displaced instructions changed");
    auto hook = safetyhook::InlineHook::create(reinterpret_cast<void*>(target_),
        reinterpret_cast<void*>(destination), safetyhook::InlineHook::StartDisabled);
    require(hook.has_value(), "disabled SafetyHook creation failed");
    hook_ = std::move(*hook);
    const auto& saved = hook_.original_bytes();
    require(saved.size() == stolen_ &&
            std::memcmp(saved.data(), before_.data(), saved.size()) == 0 &&
            hook_.trampoline().size() == stolen_ + 19 && hook_.target_address() == target_ &&
            hook_.destination_address() == destination, "unsupported SafetyHook inline layout");
    trampoline_ = hook_.trampoline().address();
    // Independently derive SafetyHook0.7's near E9/FF25 layout. Never treat
    // arbitrary observed patch or trampoline bytes as our expected ownership.
    bridge_.assign(before_.begin(), before_.begin() + stolen_);
    bridge_.resize(stolen_ + 19);
    jump(bridge_, stolen_, trampoline_ + stolen_, target_ + stolen_);
    bridge_[stolen_ + 5] = std::byte{0xff};
    bridge_[stolen_ + 6] = std::byte{0x25};
    put(bridge_, stolen_ + 11, destination);
    patched_ = before_;
    std::fill_n(patched_.begin(), stolen_, std::byte{0x90});
    jump(patched_, 0, target_, trampoline_ + stolen_ + 5);
    require(before_intact(), "disabled inline hook provenance mismatch");
}
bool InlinePatch::body_is(std::span<const std::byte> expected) const noexcept {
    const auto live = current_process_memory();
    return code_is(memory_, target_, expected) &&
           (memory_ == live || code_is(live, target_, expected));
}
bool InlinePatch::bridge_is() const noexcept {
    const auto live = current_process_memory();
    return code_is(memory_, trampoline_, bridge_, true) &&
           (memory_ == live || code_is(live, trampoline_, bridge_, true));
}
bool InlinePatch::before_intact() const noexcept { return body_is(before_) && bridge_is(); }
bool InlinePatch::intact() const noexcept { return body_is(patched_) && bridge_is(); }
void InlinePatch::enable() {
    require(before_intact(), "inline hook changed before enable");
    require(FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(trampoline_),
                                  bridge_.size()) != FALSE, "inline trampoline cache flush failed");
    const auto result = hook_.enable();
    require(result.has_value() &&
            FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(target_), stolen_) != FALSE &&
            intact(), "enabled inline hook provenance mismatch");
}
} // namespace point_zero::native::detail
