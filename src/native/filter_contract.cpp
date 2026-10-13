#include "build_bindings.hpp"
#include "filter_contract.hpp"

#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <utility>

namespace point_zero::native {
namespace {
using Pin = build_bindings::FilterContract::Pin;



// Six music IDs directly traversed by the admitted candidate wrapper.

// RTTI prefix + exactly seven level slots / two control slots. Read binding
// values, not raw pointer hashes, so relocated images are accepted.



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
    for (const auto pin : game_bindings(game.range).filter_contract.code_pins) {
        FilterSite* site = pin.rva == game_bindings(game.range).filter_contract.filter_apply_rva ? &result.sites.apply :
                           pin.rva == game_bindings(game.range).filter_contract.filter_level_rva ? &result.sites.level :
                           pin.rva == game_bindings(game.range).filter_contract.filter_candidate_rva ? &result.sites.candidate : nullptr;
        if (site) *site = {game.range.base + pin.rva, copy_checked(memory, game.range, pin, RangeUse::image_code)};
        else (void)address(memory, game.range, pin.rva, pin.size, RangeUse::image_code);
    }
    (void)address(memory, game.range, game_bindings(game.range).filter_contract.candidate_table.rva, game_bindings(game.range).filter_contract.candidate_table.size, RangeUse::image_read);
    bindings(memory, game.range, game_bindings(game.range).filter_contract.level_table_rva, game_bindings(game.range).filter_contract.level_bindings);
    bindings(memory, game.range, game_bindings(game.range).filter_contract.control_table_rva, game_bindings(game.range).filter_contract.control_bindings);
    result.sites.apply_caller = game.range.base + game_bindings(game.range).filter_contract.filter_apply_caller_rva;
    result.sites.level_caller = game.range.base + game_bindings(game.range).filter_contract.sites_level_caller_rva;
    result.mode = reinterpret_cast<decltype(result.mode)>(game.range.base + game_bindings(game.range).filter_contract.mode_rva);
    result.rebuild = reinterpret_cast<decltype(result.rebuild)>(game.range.base + game_bindings(game.range).filter_contract.rebuild_rva);
    return result;
}

// Factory bytes prove allocation size 28h, control vtable CC1540, object at
// control+10h with vtable CC1490, and shared-pair publication. No factory call,
// ownership operation or vtable mutation is performed by this contract.
// Mutable singleton slots/maps, RTTI graphs, unrelated predicate bodies,
// rebuild helpers, editors, renderers and transitive CRT/OS are outside this slice.
} // namespace point_zero::native
