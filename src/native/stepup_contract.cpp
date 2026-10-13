#include "build_bindings.hpp"
#include "stepup_contract.hpp"

#include <stdexcept>
#include <vector>

namespace point_zero::native {
namespace {
using Pin = build_bindings::StepupContract::Pin;


// Source: retained 03_bm2dx/binaries/LDJ-010-2026081900/bm2dx.dll,
// SHA256 c61b6dcb8894062e56d60da8ca90053b27f129e1a8e8da5e54457aa42602397d.
// Read as PE data, never executed. Complete half-open code extents:
//   begin   end      bytes  .pdata record  unwind   flags  next code
//   601E80  60208A     522  BAE5250        F1CE7C   0      602090
//   3430E0  343109      41  BABB67C        ECEAEC   0      343110
//   8D4EE0  8D5032     338  BB141AC        FD7130   2      8D5040 (leaf)
//   8D9540  8D9870     816  BB143A4        FD7614   2      8D9870
//   602BB0  602C0B      91  BAE5280        F1CE70   0      602C10 (leaf)
// Following every one of the PE's 57,745 .pdata entries through all unwind
// chains finds no descendants of these roots. Flag 2 is UHANDLER, not
// CHAININFO. Every direct branch remains in its body except fast_bind's
// reviewed tail jump to shared named. Its null-child branch [343103,343109)
// is included. Independent leaves 8D5040 and 602C10 start after INT3 padding;
// neither is a continuation simply because it lacks a .pdata record.
// No owned bytes overlap a base relocation. Unwind/header metadata and
// exception handlers establish provenance only, not runtime admission.
//
// Builder: RSI=owner; [RSI+10h]=layer; [RSI+44h]=difficulty;
// [RSI+48h]=native level. 601F4E loads R8 from the native number table
// using 13*difficulty+level. CALL 601F56 -> 3430E0 returns to 601F5B.
// fast_bind preserves that R8 name across child lookup 3437A0, restores
// its push RBX/sub RSP,20h frame and JMPs at 3430FE -> 340390. There is
// no extra return frame: named sees 601F5B at the builder's capture RSP-8.
//
// Constructor stores D818B8 as the scene vtable and constructs scene+518h.
// Confirmation publishes music/difficulty, then calls 602BB0 at 8D9672
// and builder at 8D967E, both with scene+518h, before its cabinet branch.
// Committed tuple stores music +38h, style +40h, difficulty +44h and
// native level +48h. Tests also retain callback/slot-initialization evidence
// outside these five bodies, without silently expanding runtime admission.
// Cross-check: research_v1_remaining_contracts_20261001/modes/{README.md,
// pe-evidence.json,owner-identities.json,disassembly}. Native lookup, table
// contents, scene callback state, slot 46 and actual label/resource identity
// remain the consuming runtime's responsibility.


void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }

std::uintptr_t address(MemoryView memory, ImageRange image, Pin pin) {
    require(image.base >= 0x10000 && contains(image.base, image.size, image.base, image.size) &&
            pin.rva <= image.size && pin.size <= image.size - pin.rva,
            "Step Up contract range outside selected image");
    const auto at = image.base + pin.rva;
    require(readable_range(memory, at, pin.size, RangeUse::image_code, image),
            "Step Up contract allocation or protection changed");
    return at;
}

PlayedFunction code(MemoryView memory, ImageRange image, Pin pin) {
    PlayedFunction result{address(memory, image, pin), std::vector<std::byte>(pin.size)};
    require(read_bytes(memory, result.address, result.body), "Step Up contract body is unreadable");
    // read_bytes checks readability, not executable selected-image ownership.
    (void)address(memory, image, pin);
    return result;
}
} // namespace

StepUpSites admit_stepup_sites(MemoryView memory, const ImageInfo& game) {
    StepUpSites result{code(memory, game.range, game_bindings(game.range).stepup_contract.pins[0]), code(memory, game.range, game_bindings(game.range).stepup_contract.pins[1]),
                       code(memory, game.range, game_bindings(game.range).stepup_contract.pins[2]), code(memory, game.range, game_bindings(game.range).stepup_contract.pins[3]),
                       code(memory, game.range, game_bindings(game.range).stepup_contract.pins[4])};
    result.capture = game.range.base + game_bindings(game.range).stepup_contract.capture_rva;
    result.named_return = game.range.base + game_bindings(game.range).stepup_contract.named_return_rva;
    return result;
}

// These observations are not atomic and cannot certify future memory.
// Hook owners must recheck the owned bodies before exposure and retain code
// ownership through use. No owner lifetime or live tuple validity is implied.
} // namespace point_zero::native
