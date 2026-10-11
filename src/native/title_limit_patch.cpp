#include "title_limit_patch.hpp"
#include "ratings/ratings.hpp"

#include <windows.h>
#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace point_zero::native::detail {
namespace {
static_assert(sizeof(std::uintptr_t) == 8 && std::endian::native == std::endian::little);
constexpr std::array before{std::byte{0x0f}, std::byte{0x2f}, std::byte{0xc1},
                            std::byte{0x76}, std::byte{0x10}};
// SafetyHook0.7.0 src/mid_hook.cpp, first (Windows x64) asm_data array.
// The 391-byte public MidHook stub has 375 fixed instruction bytes followed by
// callback and trampoline pointers. Hash derived from the upstream source,
// not from this process's allocation. Other library/architecture layouts refuse.
constexpr std::string_view stub_sha = "b99e270acb96c810c539d96db7f03fdf4e68e23a4938253582e4d8d86e07d357";

void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
template<class T> T field(std::span<const std::byte> bytes, std::size_t at) noexcept {
    T result{}; std::memcpy(&result, bytes.data() + at, sizeof(result)); return result;
}
template<class T> void put(std::span<std::byte> bytes, std::size_t at, T value) noexcept {
    std::memcpy(bytes.data() + at, &value, sizeof(value));
}
bool executable(MemoryView memory, std::uintptr_t address, std::size_t size, bool private_code) noexcept {
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
}
bool code_is(MemoryView memory, std::uintptr_t address, std::span<const std::byte> expected,
             bool private_code = false) noexcept {
    if (!executable(memory, address, expected.size(), private_code)) return false;
    std::array<std::byte, 28> actual{};
    if (expected.size() > actual.size()) return false;
    auto bytes = std::span(actual).first(expected.size());
    return read_bytes(memory, address, bytes) && std::equal(bytes.begin(), bytes.end(), expected.begin());
}
std::int32_t displacement(std::uintptr_t after, std::uintptr_t destination) {
    require(after <= INT64_MAX && destination <= INT64_MAX, "title branch address outside user range");
    const auto value = static_cast<std::int64_t>(destination) - static_cast<std::int64_t>(after);
    require(value >= (std::numeric_limits<std::int32_t>::min)() &&
            value <= (std::numeric_limits<std::int32_t>::max)(), "title branch outside rel32 range");
    return static_cast<std::int32_t>(value);
}
std::uintptr_t destination(std::uintptr_t after, std::int32_t delta) {
    require(after <= INT64_MAX, "title relay address outside user range");
    require(delta <= 0 || after <= static_cast<std::uintptr_t>(INT64_MAX - delta),
            "title relay destination overflow");
    const auto result = static_cast<std::int64_t>(after) + delta;
    require(result >= 0x10000, "title relay destination invalid");
    return static_cast<std::uintptr_t>(result);
}
} // namespace

TitleLimitPatch::TitleLimitPatch(MemoryView memory, std::uintptr_t target, safetyhook::MidHookFn callback)
    : memory_(memory), target_(target), callback_(reinterpret_cast<std::uintptr_t>(callback)) {
    require(callback_ >= 0x10000 && target_ <= INT64_MAX - 21 &&
            code_is(memory_, target_, before) &&
            (memory_ == current_process_memory() || code_is(current_process_memory(), target_, before)),
            "title comparison before-bytes or callback invalid");
    auto hook = safetyhook::MidHook::create(reinterpret_cast<void*>(target_), callback,
                                           safetyhook::MidHook::StartDisabled);
    require(hook.has_value(), "disabled title mid-hook creation failed");
    hook_ = std::move(*hook);
    require(before_intact(), "disabled title mid-hook metadata or native bytes changed");
}
bool TitleLimitPatch::before_intact() const noexcept {
    const auto& saved = hook_.original_bytes();
    return hook_ && !hook_.enabled() && hook_.target_address() == target_ &&
        reinterpret_cast<std::uintptr_t>(hook_.destination()) == callback_ &&
        saved.size() == before.size() && std::memcmp(saved.data(), before.data(), before.size()) == 0 &&
        code_is(memory_, target_, before) &&
        (memory_ == current_process_memory() || code_is(current_process_memory(), target_, before));
}
void TitleLimitPatch::identify() {
    // Public MidHook0.7 does not expose its private allocations. Use the actual
    // entry only to locate them, then reconstruct and verify EVERY owned byte.
    // Both target and backing are independently checked through current-process
    // memory as well as the supplied view; a fabricated view cannot admit code.
    const auto memory = current_process_memory();
    std::array<std::byte, 5> entry{};
    require(read_bytes(memory, target_, entry) && entry[0] == std::byte{0xe9},
            "title mid-hook is not the selected near entry");
    const auto relay = destination(target_ + 5, field<std::int32_t>(entry, 1));
    std::array<std::byte, 14> relay_bytes{};
    constexpr std::array relay_header{std::byte{0xff}, std::byte{0x25}, std::byte{},
                                      std::byte{}, std::byte{}, std::byte{}};
    require(executable(memory, relay, relay_bytes.size(), true) && read_bytes(memory, relay, relay_bytes) &&
            std::equal(relay_header.begin(), relay_header.end(), relay_bytes.begin()),
            "title mid-hook relay layout changed");
    stub_ = field<std::uintptr_t>(relay_bytes, 6);
    std::array<std::byte, 391> stub{};
    require(executable(memory, stub_, stub.size(), true) && read_bytes(memory, stub_, stub),
            "title mid-hook stub is not executable private storage");
    require(ratings::sha256({reinterpret_cast<const char*>(stub.data()), 375}) == stub_sha &&
            field<std::uintptr_t>(stub, 375) == callback_, "title mid-hook stub/callback changed");
    trampoline_ = field<std::uintptr_t>(stub, 383);
    require(trampoline_ >= 0x10000 && trampoline_ <= INT64_MAX - bridge_.size() && relay == trampoline_ + 14,
            "title mid-hook trampoline/relay relationship changed");

    // Original COMISS (3); widened JBE rel32 (6); jump back to native DIVSS
    // (5); FF25 relay plus our stub pointer (14). This is the reviewed public
    // InlineHook E9 relocation algorithm, not a snapshot of observed code.
    bridge_ = {};
    std::copy_n(before.begin(), 3, bridge_.begin());
    bridge_[3] = std::byte{0x0f}; bridge_[4] = std::byte{0x86};
    put(bridge_, 5, displacement(trampoline_ + 9, target_ + 21));
    bridge_[9] = std::byte{0xe9};
    put(bridge_, 10, displacement(trampoline_ + 14, target_ + 5));
    std::copy(relay_header.begin(), relay_header.end(), bridge_.begin() + 14);
    put(bridge_, 20, stub_);
    patched_[0] = std::byte{0xe9};
    put(patched_, 1, displacement(target_ + 5, trampoline_ + 14));
    require(backing_is(memory_) && (memory_ == memory || backing_is(memory)), "title mid-hook complete provenance mismatch");
    verified_ = true;
}
bool TitleLimitPatch::backing_is(MemoryView memory) const noexcept {
    try {
        if (!stub_ || !trampoline_ || !code_is(memory, target_, patched_) ||
            !code_is(memory, trampoline_, bridge_, true) || !executable(memory, stub_, 391, true)) return false;
        std::array<std::byte, 391> stub{};
        return read_bytes(memory, stub_, stub) &&
            ratings::sha256({reinterpret_cast<const char*>(stub.data()), 375}) == stub_sha &&
            field<std::uintptr_t>(stub, 375) == callback_ &&
            field<std::uintptr_t>(stub, 383) == trampoline_;
    } catch (...) { return false; }
}
void TitleLimitPatch::enable() {
    require(before_intact(), "title mid-hook changed before enable");
    // The public MidHook API hides allocation locators until the entry exists.
    // Its already-created code must be coherent before any thread can enter it.
    require(FlushInstructionCache(GetCurrentProcess(), nullptr, 0) != FALSE,
            "title mid-hook pre-enable cache flush failed");
    require(hook_.enable().has_value(), "title mid-hook enable failed");
    require(FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(target_), before.size()) != FALSE,
            "title mid-hook entry cache flush failed");
    identify();
    require(intact(), "title mid-hook changed during admission");
}
bool TitleLimitPatch::intact() const noexcept {
    return verified_ && hook_.enabled() && backing_is(memory_) &&
        (memory_ == current_process_memory() || backing_is(current_process_memory()));
}
} // namespace point_zero::native::detail
