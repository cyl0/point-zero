#include "build_bindings.hpp"
#include "sort_contract.hpp"

#include <array>
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace point_zero::native {
namespace {
using Pin = build_bindings::SortContract::Pin;


// Primary evidence: retained 03_bm2dx/binaries/LDJ-010-2026081900/bm2dx.dll,
// PE64/AMD64, inspected as data. No body intersects a base relocation.
// Bounds are half-open and include every chained fragment and inline table:
//   870D60..870FFC UW FB71C4; executable through 870F99, table 870F9C..870FFC.
//   B776D0..B7772B UW 104FA04; B7772B..B779E7 UW 104FA1C and
//   B779E7..B77A04 UW 104FA40 both chain to B776D0 (820 bytes total).
//   B775D0..B775E7 UW 104FA50; B775E7..B7769B UW 104FA60 and
//   B7769B..B7769C UW 104FA7C both chain to B775D0 (204 bytes total).
//   871250..87134C UW FB71EC; 871610..8717CF UW FB71D8.
//   82DFA0..82E025 leaf code, padding then six-entry table 82E028..82E040.
// The other leaves include all branches through their final RET; the other
// unwind-backed bodies have no chained descendants. Unwind metadata establishes
// fixture extents, not a runtime metadata gate or a whole-image fingerprint.
//
// At CALL 870F83 (E8 48 67 30 00), RCX/RDX/R8/R9 are qsort's four arguments,
// RSI is Category*, R15B is the dispatcher visible byte (also at 1077540).
// Visible rows have width 16; backing charts width 64. Resolved sort 2 selects
// 871250. That comparator reads chart +8 (P1 working difficulty), uses 82DFA0
// with the bank selected by 949530, and treats failed 9522B0 existence as zero.
// 5F97A0 returns chart->music, not a player selector. 871610 supplies native
// title/difficulty ties, including CRT equality; qsort is not stable.
// Return 870F88 runs only the dispatcher epilogue, leaving cursor policy to
// its callers. The hook must capture RSI/R15 before executing the qsort body.
// These extents check executable image ownership; their bodies are not copied.


void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }

std::uintptr_t address(MemoryView memory, ImageRange image, Pin pin) {
    require(image.base >= 0x10000 && contains(image.base, image.size, image.base, image.size) &&
            pin.rva <= image.size && pin.size <= image.size - pin.rva,
            "sort contract range outside selected image");
    const auto at = image.base + pin.rva;
    require(readable_range(memory, at, pin.size, RangeUse::image_code, image),
            "sort contract allocation or protection changed");
    return at;
}

// Independent instructions from the retained PE, not a live snapshot.
template<std::size_t N>
std::vector<std::byte> expected_code(MemoryView memory, ImageRange image, std::uint32_t rva,
                                    const std::array<std::byte, N>& expected) {
    const Pin pin{rva, static_cast<std::uint32_t>(N)};
    const auto at = address(memory, image, pin);
    std::vector<std::byte> body(N);
    require(read_bytes(memory, at, body) && std::equal(body.begin(), body.end(), expected.begin()),
            "sort contract selected instructions changed");
    (void)address(memory, image, pin);
    return body;
}
constexpr std::array qsort_entry{std::byte{0x40}, std::byte{0x55}, std::byte{0x41},
    std::byte{0x54}, std::byte{0x41}, std::byte{0x56}};
constexpr std::array dispatcher_call{std::byte{0xe8}, std::byte{0x48}, std::byte{0x67},
    std::byte{0x30}, std::byte{0x00}};
constexpr std::array comparator_selection{std::byte{0x48}, std::byte{0x8d}, std::byte{0x1d},
    std::byte{0x12}, std::byte{0x04}, std::byte{0x00}, std::byte{0x00}};
} // namespace

SortContract admit_sort_contract(MemoryView memory, const ImageInfo& game) {
    SortContract result;
    for (const auto pin : game_bindings(game.range).sort_contract.pins) (void)address(memory, game.range, pin);
    result.hook.body = expected_code(memory, game.range, game_bindings(game.range).sort_contract.qsort_rva, qsort_entry);
    (void)expected_code(memory, game.range, game_bindings(game.range).sort_contract.dispatcher_call_rva, dispatcher_call);
    (void)expected_code(memory, game.range, game_bindings(game.range).sort_contract.comparator_selection_rva, comparator_selection);
    const auto base = game.range.base;
    result.calls.mode = reinterpret_cast<decltype(result.calls.mode)>(base + game_bindings(game.range).sort_contract.mode_rva);
    result.calls.level = reinterpret_cast<decltype(result.calls.level)>(base + game_bindings(game.range).sort_contract.level_rva);
    result.hook.address = base + game_bindings(game.range).sort_contract.qsort_rva;
    result.hook.caller = base + game_bindings(game.range).sort_contract.sort_return_rva;
    return result;
}

// Scope: 895290 option cycling is not called by this operation. 86EF00 default
// metadata lookup may legitimately be hooked by Playlister, so it is not pinned;
// the actual incoming comparator determines applicability after native default
// resolution. GameDriver independently admits the catalogue dependency behind
// 9520A0 and shared player/style dependencies. Transitive CRT locale/security/OS
// implementations, the mutable CFG slot, global values, ownership, membership,
// cursor restoration and hook lifetime are outside this finite byte contract.
// Admission checks bounds/permissions and the selected independent instructions.
// Retain the module and recheck displaced-byte stability when constructing/arming
// the hook; callers serialize native use. Future mutation is not certified.
} // namespace point_zero::native
