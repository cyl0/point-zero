#include "list_hook_contract.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <vector>

namespace point_zero::native {
namespace {
struct Pin { std::uint32_t rva, size; };

// Evidence: retained 03_bm2dx/binaries/LDJ-010-2026081900/bm2dx.dll,
// read as PE data. All ranges are half-open RVAs, exclude following INT3
// padding, and contain no base relocations. Unwind metadata establishes the
// fixture extents; it is not a runtime gate or a whole-image fingerprint.
//
// Shared named-image emitter [34AC50,34AFA8), disk [34A050,34A3A8).
// Seven adjacent RUNTIME_FUNCTION records at [BABBDFC,BABBE50):
//   begin   end      unwind   parent begin (CHAININFO)
//   34AC50  34AC7E   ECF66C   root
//   34AC7E  34AC89   ECF67C   34AC50
//   34AC89  34AE53   ECF690   34AC7E
//   34AE53  34AEEF   ECF6AC   34AC7E
//   34AEEF  34AF1D   ECF6BC   34AC50
//   34AF1D  34AF96   ECF6CC   34AC50
//   34AF96  34AFA8   ECF6EC   34AC50
// The first ret is 34AEEE, the overflow ret 34AF1C. Virtual fallback paths
// continue through the jump at 34AFA3 back to 34AED6. The next independent
// function starts at 34AFB0. These are static ABI extents, not byte identity checks.
//
// Complete background caller [5F7BF0,5F7FD5), disk [5F6FF0,5F73D5).
// Five records at [BAE468C,BAE46C8) have exclusive ends
// 5F7C1C,5F7C37,5F7DCF,5F7FC4,5F7FD5 and unwind RVAs
// F19ECC,F19EE0,F19F04,F19F1C,F19F2C. Parents by index: -,0,1,1,0.
// Next independent function: 5F7FE0. RSI receives entry RCX at 5F7C10 and
// retains the row owner through CALL 5F7E5D (return address 5F7E62).
// RDX selects listb/listb_secret/listb_lightning via RIP-relative LEAs;
// RCX is the result of 34A610, R8D is adjusted x, R9D is y. Outgoing stack
// +20 holds float alpha bits, +28 the layer (input +5), +30 zero flags.
//
// Complete level caller [5F8570,5F877D), disk [5F7970,5F7B7D).
// Three records at [BAE4740,BAE4764) end at 5F859F,5F86D3,5F877D;
// unwind RVAs F1A018,F1A02C,F1A048; both children chain to 5F8570.
// First ret 5F86D2 precedes the solo path; next function starts at 5F8780.
// RSI receives RCX at 5F8589. [RSI] supplies music, +8/+C difficulties;
// style/level come from 949530/893930. The index is 13*difficulty+level.
// Return 5F865C uses compact table 105D300 (P1); 5F86AB uses compact P2
// or ordinary 105D0F0 when difficulties match; 5F876D uses ordinary and
// active player 949230. RCX comes from 34A610, RDX from the selected table,
// R8D=x+29, R9D=y+5/+26/+11; stack +20/+28/+30 = alpha/layer/zero flags.
// At emitter entry those stack slots are +28/+30/+38. Alpha is preserved
// as raw uint32_t float bits in ListHook::Draw, not numerically converted.
//
// Cross-checked against retained research_v1_playlister51_20261001/candidate/
// source/src/{native_resource_bindings.hpp,list_ux_native_pins.inl,
// final_surface_pins.hpp}. list_glyph_pins.inl includes legacy offsets;
// none were transplanted into the final-image contract.
constexpr Pin pins[]{
    {0x34AC50, 856},
    {0x5F7BF0, 997},
    {0x5F8570, 525},
};

void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }

std::uintptr_t address(MemoryView memory, ImageRange image, Pin pin) {
    require(contains(image.base, image.size, image.base, image.size) &&
            pin.rva <= image.size && pin.size <= image.size - pin.rva,
            "list hook contract range outside selected image");
    const auto at = image.base + pin.rva;
    require(readable_range(memory, at, pin.size, RangeUse::image_code, image),
            "list hook contract allocation or protection changed");
    return at;
}

ListHookSite code(MemoryView memory, ImageRange image, Pin pin) {
    // Whole displaced instructions: MOV RAX,RSP; MOV [RAX+20h],RBP.
    constexpr std::array expected{std::byte{0x48}, std::byte{0x8b}, std::byte{0xc4},
        std::byte{0x48}, std::byte{0x89}, std::byte{0x68}, std::byte{0x20}};
    ListHookSite result{address(memory, image, pin), std::vector<std::byte>(expected.size())};
    require(read_bytes(memory, result.address, result.body) &&
            std::equal(result.body.begin(), result.body.end(), expected.begin()),
            "list hook contract displaced instructions changed");
    // Re-query image ownership/execution after the read as well: read_bytes
    // deliberately checks readability only and does not enforce image use.
    (void)address(memory, image, pin);
    return result;
}
} // namespace

ListHookSite admit_list_hook_site(MemoryView memory, const ImageInfo& game) {
    auto result = code(memory, game.range, pins[0]);
    (void)address(memory, game.range, pins[1]);
    (void)address(memory, game.range, pins[2]);
    const auto base = game.range.base;
    result.callers = {base + 0x5F7E62, base + 0x5F865C, base + 0x5F86AB, base + 0x5F876D};
    return result;
}

// Limits: observation is not atomic and cannot certify future mutable memory.
// Construction/arm rechecks the displaced emitter instructions; callers must
// remain stable through use. Caller ABI is static retained evidence, not table
// contents/strings, live row/chart/pool state, imported/virtual targets or the
// transitive implementations of callees. Those need their own admission and
// per-draw validation. Title code at 5F8AC0 is deliberately not admitted here.
} // namespace point_zero::native
