#include "filter_contract.hpp"

#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <utility>

namespace point_zero::native {
namespace {
struct Pin { std::uint32_t rva, size; };

constexpr std::array<Pin, 9> code_pins{{
    {0x6E9010,131},
    {0x6E8E40,32},
    {0x893FA0,506},
    {0x9493C0,7},
    {0x885440,8},
    {0x883290,8},
    {0x68B780,541},
    {0x82DFA0,160},
    {0x5FD730,1172},
}};
// Six music IDs directly traversed by the admitted candidate wrapper.
constexpr Pin candidate_table{0xD57FC8,24};
// RTTI prefix + exactly seven level slots / two control slots. Read binding
// values, not raw pointer hashes, so relocated images are accepted.
constexpr std::array<std::uint32_t, 8> level_bindings{
    0xE9B0D0,0x68BCA0,0x6E8E40,0x63E9C0,0x68BDE0,0x68BDF0,0x68C010,0x68C020};
constexpr std::array<std::uint32_t, 3> control_bindings{0xE9B1D8,0x68C050,0x68C030};

void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
std::uintptr_t address(MemoryView memory, ImageRange image, std::uint32_t rva,
                       std::size_t size, RangeUse use) {
    require(image.base >= 0x10000 && contains(image.base, image.size, image.base, image.size) &&
            rva <= image.size && size <= image.size - rva,
            "filter contract range outside selected image");
    const auto at = image.base + rva;
    require(readable_range(memory, at, size, use, image),
            "filter contract allocation or protection changed");
    return at;
}
std::vector<std::byte> copy(MemoryView memory, ImageRange image, std::uint32_t rva,
                            std::size_t size, RangeUse use) {
    const auto at = address(memory, image, rva, size, use);
    std::vector<std::byte> bytes(size);
    require(read_bytes(memory, at, bytes), "filter contract copy unreadable");
    // read_bytes alone does not verify image allocation or executability.
    // Recheck each required copied range after the read.
    (void)address(memory, image, rva, size, use);
    return bytes;
}
std::vector<std::byte> copy_checked(MemoryView memory, ImageRange image, Pin pin, RangeUse use) {
    auto bytes = copy(memory, image, pin.rva, pin.size, use);
    return bytes;
}
template<std::size_t N>
void bindings(MemoryView memory, ImageRange image, std::uint32_t rva,
              const std::array<std::uint32_t, N>& expected) {
    const auto bytes = copy(memory, image, rva, N * sizeof(std::uintptr_t), RangeUse::image_read);
    for (std::size_t i = 0; i < N; ++i) {
        std::uintptr_t actual{};
        std::memcpy(&actual, bytes.data() + i * sizeof(actual), sizeof(actual));
        const auto target = address(memory, image, expected[i], 1,
                                    i ? RangeUse::image_code : RangeUse::image_read);
        require(actual == target, "filter contract vtable or RTTI binding changed");
    }
}
} // namespace

FilterContract admit_filter_contract(MemoryView memory, const ImageInfo& game) {
    static_assert(sizeof(std::uintptr_t) == 8);
    FilterContract result;
    for (const auto pin : code_pins) {
        FilterSite* site = pin.rva == 0x6E9010 ? &result.sites.apply :
                           pin.rva == 0x6E8E40 ? &result.sites.level :
                           pin.rva == 0x893FA0 ? &result.sites.candidate : nullptr;
        if (site) *site = {game.range.base + pin.rva, copy_checked(memory, game.range, pin, RangeUse::image_code)};
        else (void)address(memory, game.range, pin.rva, pin.size, RangeUse::image_code);
    }
    (void)address(memory, game.range, candidate_table.rva, candidate_table.size, RangeUse::image_read);
    bindings(memory, game.range, 0xCC1488, level_bindings);
    bindings(memory, game.range, 0xCC1538, control_bindings);
    result.sites.apply_caller = game.range.base + 0x89407A;
    result.sites.level_caller = game.range.base + 0x6E9063;
    result.mode = reinterpret_cast<decltype(result.mode)>(game.range.base + 0x9493C0);
    result.rebuild = reinterpret_cast<decltype(result.rebuild)>(game.range.base + 0x5FD730);
    return result;
}

// 951E80 -> 951FD0/951E40 is already admitted by GameDriver's
// check_catalog_contract, including the proven Omni LEA->MOV replacements,
// catalogue allocation and index provenance. A stock-only pin here would
// incorrectly reject that supported path; this API does not admit it anew.
// Factory bytes prove allocation size 28h, control vtable CC1540, object at
// control+10h with vtable CC1490, and shared-pair publication. No factory call,
// ownership operation or vtable mutation is performed by this contract.
// Mutable singleton slots/maps, RTTI graphs, unrelated predicate bodies,
// rebuild helpers, editors, renderers and transitive CRT/OS are outside this slice.
} // namespace point_zero::native
