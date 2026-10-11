#include "played_mid_patch.hpp"
#include "ratings/ratings.hpp"

#include <windows.h>
#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace point_zero::native::detail {
namespace {
static_assert(sizeof(std::uintptr_t) == 8 && std::endian::native == std::endian::little);
static_assert(offsetof(safetyhook::Context, rsp) == 384 &&
              offsetof(safetyhook::Context, trampoline_rsp) == 392 &&
              offsetof(safetyhook::Context, rip) == 400 && sizeof(safetyhook::Context) == 408);
constexpr std::array confirmation{std::byte{0x48}, std::byte{0x8b}, std::byte{0x94}, std::byte{0xd3},
                                  std::byte{0xf0}, std::byte{0xdf}, std::byte{0x05}, std::byte{0x01}};
constexpr std::array hud{std::byte{0x48}, std::byte{0x63}, std::byte{0xd0},
                         std::byte{0x49}, std::byte{0x8b}, std::byte{0xcc}};
constexpr std::array stepup{std::byte{0x4f}, std::byte{0x8b}, std::byte{0x84}, std::byte{0xc7},
                            std::byte{0xc0}, std::byte{0xe2}, std::byte{0x05}, std::byte{0x01}};
constexpr std::array result{std::byte{0x48}, std::byte{0x63}, std::byte{0xd8}, std::byte{0xe8}};
constexpr std::array summary{std::byte{0x4c}, std::byte{0x89}, std::byte{0x74}, std::byte{0x24}, std::byte{0x38}};
constexpr std::array category_back{std::byte{0x41}, std::byte{0x80}, std::byte{0x7f}, std::byte{0x08}, std::byte{0}};
constexpr std::array boot{std::byte{0x48}, std::byte{0xc7}, std::byte{0x44}, std::byte{0x24},
                         std::byte{0x20}, std::byte{}, std::byte{}, std::byte{}, std::byte{}};
constexpr std::array guide{std::byte{0x49}, std::byte{0x8b}, std::byte{0x17},
                          std::byte{0x48}, std::byte{0x8b}, std::byte{0xc8}};
constexpr std::array relay_header{std::byte{0xff}, std::byte{0x25}, std::byte{},
                                  std::byte{}, std::byte{}, std::byte{}};
// SafetyHook0.7.0 src/mid_hook.cpp, first (Windows x64) asm_data array:
// 375 fixed instruction bytes, then callback at 375 and trampoline at 383.
// Derived from upstream source, never from observed allocation bytes.
constexpr std::string_view stub_sha = "b99e270acb96c810c539d96db7f03fdf4e68e23a4938253582e4d8d86e07d357";

void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
template<class T> T field(std::span<const std::byte> bytes, std::size_t at) noexcept {
    T value{}; std::memcpy(&value, bytes.data() + at, sizeof(value)); return value;
}
template<class T> void put(std::span<std::byte> bytes, std::size_t at, T value) noexcept {
    std::memcpy(bytes.data() + at, &value, sizeof(value));
}
bool starts_with(std::span<const std::byte> bytes, std::span<const std::byte> prefix) noexcept {
    return bytes.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), bytes.begin());
}
std::size_t stolen_size(std::span<const std::byte> bytes) {
    if (starts_with(bytes, confirmation)) return 8;
    if (starts_with(bytes, stepup)) return 8;
    if (starts_with(bytes, hud)) return 6;
    if (starts_with(bytes, summary)) return 5;
    if (starts_with(bytes, category_back)) return 5;
    if (starts_with(bytes, boot)) return 9;
    if (starts_with(bytes, guide)) return 6;
    if (bytes.size() >= 8 && starts_with(bytes, result)) return 8;
    if (bytes.size() >= 5 && bytes[0] == std::byte{0xe8}) return 5;
    throw std::runtime_error("unsupported played capture instructions");
}
bool executable(MemoryView memory, std::uintptr_t address, std::size_t size, bool private_code = false) noexcept {
    __try {
        if (!memory.read || !memory.query || address < 0x10000 || !size || size > UINTPTR_MAX - address)
            return false;
        const auto end = address + size;
        for (auto cursor = address; cursor < end;) {
            MemoryRegion region{};
            if (!memory.query(memory.context, cursor, region) ||
                !contains(region.begin, region.size, cursor, 1) ||
                !region.committed || !region.readable || !region.executable || region.guarded ||
                (private_code && region.kind != MemoryKind::private_memory)) return false;
            cursor = (std::min)(end, region.begin + region.size);
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool code_is(MemoryView memory, std::uintptr_t address, std::span<const std::byte> expected,
             bool private_code = false) noexcept {
    __try {
        if (!executable(memory, address, expected.size(), private_code)) return false;
        std::array<std::byte, 256> buffer{};
        for (std::size_t offset = 0; offset < expected.size();) {
            const auto count = (std::min)(buffer.size(), expected.size() - offset);
            auto bytes = std::span(buffer).first(count);
            if (!read_bytes(memory, address + offset, bytes) ||
                !std::equal(bytes.begin(), bytes.end(), expected.begin() + offset)) return false;
            offset += count;
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
std::int32_t displacement(std::uintptr_t after, std::uintptr_t destination) {
    require(after <= INT64_MAX && destination <= INT64_MAX, "played branch address outside user range");
    const auto value = static_cast<std::int64_t>(destination) - static_cast<std::int64_t>(after);
    require(value >= (std::numeric_limits<std::int32_t>::min)() &&
            value <= (std::numeric_limits<std::int32_t>::max)(), "played branch outside rel32 range");
    return static_cast<std::int32_t>(value);
}
std::uintptr_t destination(std::uintptr_t after, std::int32_t delta) {
    require(after <= INT64_MAX &&
            (delta <= 0 || after <= static_cast<std::uintptr_t>(INT64_MAX - delta)),
            "played branch destination overflow");
    const auto value = static_cast<std::int64_t>(after) + delta;
    require(value >= 0x10000, "played branch destination invalid");
    return static_cast<std::uintptr_t>(value);
}
} // namespace

PlayedMidPatch::PlayedMidPatch(MemoryView memory, std::uintptr_t target,
                             std::span<const std::byte> before, safetyhook::MidHookFn callback)
    : memory_(memory), target_(target), callback_(reinterpret_cast<std::uintptr_t>(callback)),
      before_(before.begin(), before.end()), stolen_(stolen_size(before_)) {
    require(callback_ >= 0x10000 && target_ <= INT64_MAX &&
            before_.size() <= static_cast<std::uintptr_t>(INT64_MAX) - target_ &&
            code_is(memory_, target_, before_) &&
            (memory_ == current_process_memory() || code_is(current_process_memory(), target_, before_)),
            "played capture before-bytes or callback invalid");
    if (before_[0] == std::byte{0xe8} || starts_with(before_, result)) {
        // Both reviewed calls end at the end of the stolen range. Never infer
        // a callee from runtime trampoline bytes or a fixed fixture address.
        call_target_ = destination(target_ + stolen_, field<std::int32_t>(before_, stolen_ - 4));
        require((call_target_ < target_ || call_target_ >= target_ + stolen_) &&
                executable(memory_, call_target_, 1) &&
                (memory_ == current_process_memory() || executable(current_process_memory(), call_target_, 1)),
                "played original near-call target invalid");
    }
    patched_ = before_;
    auto hook = safetyhook::MidHook::create(reinterpret_cast<void*>(target_), callback,
                                           safetyhook::MidHook::StartDisabled);
    require(hook.has_value(), "disabled played mid-hook creation failed");
    hook_ = std::move(*hook);
    require(before_intact(), "disabled played mid-hook metadata or native bytes changed");
}
bool PlayedMidPatch::metadata_is() const noexcept {
    const auto& saved = hook_.original_bytes();
    return hook_ && hook_.target_address() == target_ &&
        reinterpret_cast<std::uintptr_t>(hook_.destination()) == callback_ && saved.size() == stolen_ &&
        std::memcmp(saved.data(), before_.data(), stolen_) == 0;
}
bool PlayedMidPatch::before_intact() const noexcept {
    return metadata_is() && !hook_.enabled() &&
        code_is(memory_, target_, before_) &&
        (memory_ == current_process_memory() || code_is(current_process_memory(), target_, before_)) &&
        (!call_target_ || (executable(memory_, call_target_, 1) &&
                          (memory_ == current_process_memory() || executable(current_process_memory(), call_target_, 1))));
}
void PlayedMidPatch::identify() {
    // Public MidHook exposes no backing locator before enable. Entry/relay
    // bytes are locators only: every accepted byte is independently derived.
    const auto memory = current_process_memory();
    std::array<std::byte, 5> entry{};
    require(read_bytes(memory, target_, entry) && entry[0] == std::byte{0xe9},
            "played mid-hook is not the selected near entry");
    const auto relay = destination(target_ + 5, field<std::int32_t>(entry, 1));
    std::array<std::byte, 14> relay_bytes{};
    require(executable(memory, relay, relay_bytes.size(), true) && read_bytes(memory, relay, relay_bytes) &&
            starts_with(relay_bytes, relay_header), "played mid-hook relay layout changed");
    stub_ = field<std::uintptr_t>(relay_bytes, 6);
    std::array<std::byte, 391> stub{};
    require(executable(memory, stub_, stub.size(), true) && read_bytes(memory, stub_, stub),
            "played mid-hook stub is not executable private storage");
    require(ratings::sha256({reinterpret_cast<const char*>(stub.data()), 375}) == stub_sha &&
            field<std::uintptr_t>(stub, 375) == callback_, "played mid-hook stub/callback changed");
    trampoline_ = field<std::uintptr_t>(stub, 383);
    require(trampoline_ >= 0x10000 && trampoline_ <= INT64_MAX - bridge_.size() &&
            relay == trampoline_ + stolen_ + 5, "played mid-hook trampoline/relay relationship changed");

    // SafetyHook0.7 InlineHook::e9_hook copies these instructions unchanged,
    // except a relative imm32 (E8) is rewritten in place to the ORIGINAL callee.
    // It appends E9 back to target+stolen, then FF25 + stub pointer (19 bytes).
    // enable emits E9 to that relay and NOP-fills the rest of the stolen range.
    std::copy_n(before_.begin(), stolen_, bridge_.begin());
    if (call_target_) put(bridge_, stolen_ - 4, displacement(trampoline_ + stolen_, call_target_));
    bridge_[stolen_] = std::byte{0xe9};
    put(bridge_, stolen_ + 1, displacement(trampoline_ + stolen_ + 5, target_ + stolen_));
    std::copy(relay_header.begin(), relay_header.end(), bridge_.begin() + stolen_ + 5);
    put(bridge_, stolen_ + 11, stub_);
    std::fill_n(patched_.begin(), stolen_, std::byte{0x90});
    patched_[0] = std::byte{0xe9};
    put(patched_, 1, displacement(target_ + 5, relay));
    require(backing_is(memory_) && (memory_ == memory || backing_is(memory)), "played mid-hook complete provenance mismatch");
    verified_ = true;
}
bool PlayedMidPatch::backing_is(MemoryView memory) const noexcept {
    try {
        if (!stub_ || !trampoline_ || !code_is(memory, target_, patched_) ||
            !code_is(memory, trampoline_, std::span(bridge_).first(stolen_ + 19), true) ||
            !executable(memory, stub_, 391, true) ||
            (call_target_ && !executable(memory, call_target_, 1))) return false;
        std::array<std::byte, 391> stub{};
        return read_bytes(memory, stub_, stub) &&
            ratings::sha256({reinterpret_cast<const char*>(stub.data()), 375}) == stub_sha &&
            field<std::uintptr_t>(stub, 375) == callback_ &&
            field<std::uintptr_t>(stub, 383) == trampoline_;
    } catch (...) { return false; }
}
void PlayedMidPatch::enable() {
    require(before_intact(), "played mid-hook changed before enable");
    // Owner has already retained this patch permanently before this attempt.
    require(FlushInstructionCache(GetCurrentProcess(), nullptr, 0) != FALSE,
            "played mid-hook pre-enable cache flush failed");
    require(hook_.enable().has_value(), "played mid-hook enable failed");
    require(FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(target_), stolen_) != FALSE,
            "played mid-hook entry cache flush failed");
    identify();
    require(intact(), "played mid-hook changed during admission");
}
bool PlayedMidPatch::intact() const noexcept {
    return verified_ && metadata_is() && hook_.enabled() &&
        backing_is(memory_) && (memory_ == current_process_memory() || backing_is(current_process_memory()));
}
} // namespace point_zero::native::detail
