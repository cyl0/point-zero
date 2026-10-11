#include "played_contract.hpp"

#include <stdexcept>
#include <vector>

namespace point_zero::native {
namespace {
struct Pin { std::uint32_t rva, size; };

// Source: retained 03_bm2dx/binaries/LDJ-010-2026081900/bm2dx.dll,
// SHA256 c61b6dcb8894062e56d60da8ca90053b27f129e1a8e8da5e54457aa42602397d.
// Read as PE data, never executed. Complete half-open code extents:
//   begin   end      bytes  .pdata record  unwind   flags  next function
//   340390  34043B    171   BABB07C        ECE498   0      340440
//   601610  601D9B   1931   BAE5238        F1BFB8   3      601DA0
//   904470  90464E    478   BB1657C        FDEDE0   0      904650
//   91C120  91C615   1269   BB178D8        FE4C84   3      91C620
//   834510  834542     50   BB09AA8        FAA6F4   0      834550
//   925980  9259A4     36   BB182D4        FE7020   0      root
//   9259A4  9259DC     56   BB182E0        FE7028   4      chained
//   9259DC  925A0B     47   BB182EC        FE703C   4      925A10
// Flags 3 are EHANDLER|UHANDLER, not CHAININFO. Following every .pdata
// entry's unwind chain finds no descendants of these five roots. All end
// in RET before INT3 padding and the next independent function; none of
// the code bytes overlaps a base relocation. Extents are not inferred from
// an April prefix or first return. In particular, named's early RET at
// 3403B1 precedes its non-null-name branch [3403B2,34043B).
// NOTES includes all three contiguous unwind fragments, 139 bytes total.
// Unwind/header data establishes fixture provenance, not runtime admission.
//
// Confirmation captures 601715/601749 precede native table loads; named
// calls 601720/601754 return to 601725/601759. HUD capture 9045FD is the
// native-level call return; named call 90460B returns to 904610. Result
// capture 91C56D precedes RBX's style-to-level overwrite. Its layer helper
// 834510 calls named at 834537, returning at 83453C; outer result returns
// 91C5A5/91C5BA are not named returns. 83453C alone cannot establish scope.
// Cross-checks: 07_r3_cadence_probe/src/final_surface_pins.hpp and retained
// research_v1_remaining_contracts_20261001/modes/pe-evidence.json plus
// ordinary_decide, hud and ordinary_result disassemblies. Transitive helper
// implementations, imported targets, stock tables and live owners excluded.
constexpr Pin pins[]{
    {0x340390, 171},
    {0x601610, 1931},
    {0x904470, 478},
    {0x91C120, 1269},
    {0x834510, 50},
    {0x925980, 139},
};

void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }

std::uintptr_t address(MemoryView memory, ImageRange image, Pin pin) {
    require(image.base >= 0x10000 && contains(image.base, image.size, image.base, image.size) &&
            pin.rva <= image.size && pin.size <= image.size - pin.rva,
            "played contract range outside selected image");
    const auto at = image.base + pin.rva;
    require(readable_range(memory, at, pin.size, RangeUse::image_code, image),
            "played contract allocation or protection changed");
    return at;
}

PlayedFunction code(MemoryView memory, ImageRange image, Pin pin) {
    PlayedFunction result{address(memory, image, pin), std::vector<std::byte>(pin.size)};
    require(read_bytes(memory, result.address, result.body), "played contract body is unreadable");
    // read_bytes checks readability, not executable selected-image ownership.
    (void)address(memory, image, pin);
    return result;
}
} // namespace

PlayedSites admit_played_sites(MemoryView memory, const ImageInfo& game) {
    PlayedSites result{code(memory, game.range, pins[0]), code(memory, game.range, pins[1]),
                       code(memory, game.range, pins[2]), code(memory, game.range, pins[3]),
                       code(memory, game.range, pins[4])};
    result.notes_update = code(memory, game.range, pins[5]);
    const auto base = game.range.base;
    result.captures = {base + 0x601715, base + 0x601749, base + 0x9045FD, base + 0x91C56D};
    result.named_returns = {base + 0x601725, base + 0x601759, base + 0x904610, base + 0x83453C};
    result.notes_capture = base + 0x92599A;
    result.result_returns = {base + 0x91C5A5, base + 0x91C5BA};
    return result;
}

// Observations are not atomic and cannot certify future memory. Hook owners
// must recheck these owned bodies before exposure and retain code ownership
// through use. Exceptions' handlers and all unpinned callees/resources remain
// outside this contract; no tuple validity or caller-lifetime claim is made.
} // namespace point_zero::native
