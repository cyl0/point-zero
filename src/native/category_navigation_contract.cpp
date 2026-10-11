#include "category_navigation_contract.hpp"

#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <utility>

namespace point_zero::native {
namespace {
struct Pin { std::uint32_t begin, end; };
// Retained 03_bm2dx/binaries/LDJ-010-2026081900/bm2dx.dll SHA256:
// c61b6dcb8894062e56d60da8ca90053b27f129e1a8e8da5e54457aa42602397d.
// First six are complete half-open .pdata extents, including input/cycle error
// tails after their ordinary RETs. Each has ONE runtime record, no chained descendants,
// and no DIR64 overlap. UW in pin order: F1D5C8 flags2, F2DF14 flags0,
// F1D230 flags3, F09CDC flags0, F1D608 flags2, F0EC40 flags0 (all version1).
// Handler flags2/3 are not CHAININFO. In particular nearby 603670 is a
// DIFFERENT root with fragments ending 60371A/603777/603787, not open/close.
// Tests independently traverse all exception/relocation records OFFLINE;
// runtime never scans those directories or the whole image.
// Exact adds THREE fragments ending 5FD561/5FD59D/5FD606 (UW F1AF6C/7C/90).
// Restore is the complete 13-byte leaf, no .pdata; set-difficulty adds THREE
// fragments ending 5FDC07/5FDC46/5FDC68 (UW F1AED4/E8/FC). No DIR64 overlap.
constexpr std::array<Pin, 9> pins{{
    {0x603900, 0x603A12},
    {0x63A940, 0x63A9FC},
    {0x604110, 0x604C66},
    {0x594A20, 0x594AAE},
    {0x603810, 0x6038F4},
    {0x5C5C80, 0x5C5CBB},
    {0x5FD520, 0x5FD606},
    {0x5F7BE0, 0x5F7BED},
    {0x5FDBD0, 0x5FDC68},
}};

// ABI/frame evidence: open/close RCX=navigation, DL=sound, preserve R14 and
// RBX/RBP/RSI/RDI, push8+sub40h (48h stack delta); neither returns a status.
// Cycle RCX=list, EAX=0/5, push8+sub20h, preserves RBX/RSI/RDI. Its +38h
// virtual predicate receives RDX=&list+7Ch, R8D=ID count for style(+78h).
// Success plays 28C0, writes list+84h, returns5; bounds failures include INT3.
// Pressed ECX=mask returns AL (both paths zero-extend); native Back uses
// decimal5418 == 152Ah, NOT 5418h. It combines per-player results equal to1.
// Sound ECX=int ID: 5C5C86 OVERWRITES EDX from ECX before any call; old callers'
// EDX=50 is ignored. Push RBX/sub20h; RAX is merely the external call's result.
// Input keeps navigation in R15, category in R14, event in ESI; pushes five
// registers/sub110h (138h total). Its UW FrameRegister=0 despite using RBP.
//
// Closed branch: nonnull category, nav+5==0, mode!=5 and
// (held(152A) || nav+4==0) reach 6042BA call [list vtable+8]. Return address
// 6042BD captures EAX into ESI. D53278+8 is DIR64 -> 63A940. nav+8 clears;
// event5 re-resolves selection. With the normal nav+4=1, this cycle call is
// reached only AFTER the native hold threshold, not on a fresh black-key edge.
// Consequently cycle observes group changes but cannot implement short Back.
// Pending +70h handling can synthesize event14 ONLY if ESI==0. Nonzero 3/4
// skip difficulty handling at 60462C and go to final 7FB4A0/6056D0 bookkeeping,
// then EAX=ESI at 604BBA.
//
// Short release follows native's other closed-heading branch: 604463 records
// pressed(152A) in nav+8, white-key pressed(2AD5) at604476 has priority, then
// released(152A) at604515 must succeed before60451E CMP BYTE PTR[R15+8],0.
// This exact five-byte instruction is the Back capture. Native held-cycle
// clears nav+8, so a long hold does not become Back on release. A private closed
// child with nav+8=1 consumes this latch, calls close(nav,1), leaves the page,
// sets ESI=4, and resumes604B9F. That existing native completion only uses the
// event and reloads the current selection for presentation; cached R14 is not
// dereferenced there. Unowned/unsupported contexts execute the original CMP
// and native sort-release path unchanged. No CALL is displaced into a trampoline.
//
// Open tail calls 870D60, then (except ID36h) 5FD230 and sets nav+5=1;
// it stores manager+2A420=current ID. Null selection skips this entire tail.
// Input's 604501 call to open is followed by ESI=3 regardless of open's RAX.
// A private parent enter intentionally replaces this tail, keeps heading
// closed and supplies its own sound2C80; native leaf opens must forward.
// Close resolves current category and delegates to 5FC190: category+4=0,
// cursor+14h=0, auxiliary+ A0h reset via 68DF60, optional sound2C80 tail call.
// Close then clears 7FB4B0()+4 and nav+5 even if the category was null. It does
// not refresh/restart previews or clear manager+2A420. These immediate helper
// semantics are checked OFFLINE, not recursively certified at runtime.
// 6060B0's missing-ID path also calls close unconditionally (DL=0 there).
// 7FB4A0 is simply AL=(EDX!=0), so both event3 and cycle-Back4 reach 6056D0.
// That helper reloads CURRENT category via 603BF0, then reads its bar vector;
// it holds no old group-vector row across enter/leave. Caller must retain the
// category bank and provide a valid nonnull current category/cursor. It emits
// presentation TEXT via 971D70, not working preferences/graph reconciliation.
// Its actual five chained extents end 6056FB/605717/6057E1/6057E9/605801.
// The input's event3/4 continuation does not dereference cached R14/category
// or its old selection index before that fresh lookup. Leaf open still follows
// native preference setters/sort/open and manager last-open-ID update. No new
// helper call or hook is inferred from the text-presentation path alone.
// Broader presentation is AFTER input returns: 6077D0 -> 608060 -> 6084A0
// reloads current category, treats events3/4 as heading changes (3..5), resets
// chart panels and updates heading text/counts. Mask123Ch includes events3/4;
// difficulty highlights read navigation+Ch/+10h via603ED0 -> 600A10. This does
// not copy working preferences into category rows; native leaf-open 5FDBD0
// handles that. Keep text/heading presentation distinct from graph calculation;
// no extra graph refresh is justified by this finite trace. These outer bodies
// and all their callees remain OFFLINE evidence, outside the admission pins.
//
// Retained PL bm2dx.cc/playlister.cc discovery review finds no selected hook.
// Its hk_close_categories is 86EC40 (signature call603F0E), not close603810.
// Metadata/lookup/active accessors called here can have separately admitted PL
// routes. Known Fervidex input patch8EC6F8 is elsewhere (retained adapter
// evidence only); neither observation asserts whole-module compatibility.
// External lookup/style/input/virtual/filter/sound internals, shared-owner
// release, EH/CRT and mutable singleton contents remain caller dependencies.
// Exact RCX=category, EDX=song, R8D=P1, R9D=P2 returns AL; upper RAX is not
// a boolean. 605C2E (return605C33) is its only retained direct call. Native
// 5FD4A0 searches SONG ONLY; metadata+9 then allows5F9B00 to retarget that row.
// The private-leaf replacement must match original tuples before mutation.
// Restore5F7BE0 has no calls and copies both original fields to working fields.
// Set5FDBD0 writes category+18+4*player before the metadata lookup; nonzero
// metadata+9 skips the chart loop and only writes header/random +3C/+6C.
// Zero metadata+9 reaches5F9B00 for every chart. 86EF00 metadata admission and
// routing (including Playlister) remain the existing separate prerequisite.
// Native exact forwarding dependencies5FD4A0/5FC440/5F9B00, singleton883280,
// their callees and caller6058A0 are OFFLINE evidence only in this contract.
// Preserve ordinary forwarding rather than add hooks at those helpers. This
// contract neither admits request-consumer hooks nor certifies live coexistence.
constexpr std::uint32_t cycle_slot = 0xD53280;

void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
std::uintptr_t address(ImageRange image, std::uint32_t rva, std::size_t size) {
    require(image.base >= 0x10000 && contains(image.base, image.size, image.base, image.size) &&
            rva <= image.size && size <= image.size - rva,
            "category navigation contract extent outside selected image");
    return image.base + rva;
}
std::vector<std::byte> copy(MemoryView memory, ImageRange image, std::uint32_t rva,
                            std::size_t size, RangeUse use) {
    const auto at = address(image, rva, size);
    require(readable_range(memory, at, size, use, image),
            "category navigation contract ownership/protection changed");
    std::vector<std::byte> bytes(size);
    require(read_bytes(memory, at, bytes), "category navigation contract read failed");
    require(readable_range(memory, at, size, use, image),
            "category navigation contract changed during read");
    return bytes;
}
struct Observation {
    std::array<std::vector<std::byte>, pins.size()> bodies;
};
Observation observe(MemoryView memory, ImageRange image) {
    Observation result;
    for (std::size_t i = 0; i < pins.size(); ++i) {
        const auto pin = pins[i];
        auto& bytes = result.bodies[i];
        if (i == 0 || i == 1 || i == 2 || i == 6)
            bytes = copy(memory, image, pin.begin, pin.end - pin.begin, RangeUse::image_code);
        else {
            const auto at = address(image, pin.begin, pin.end - pin.begin);
            require(readable_range(memory, at, pin.end - pin.begin, RangeUse::image_code, image),
                    "category navigation support ownership/protection changed");
        }
    }
    const auto binding = copy(memory, image, cycle_slot, sizeof(std::uintptr_t), RangeUse::image_read);
    std::uintptr_t target{};
    std::memcpy(&target, binding.data(), sizeof(target));
    require(target == address(image, pins[1].begin, pins[1].end - pins[1].begin),
            "category navigation list vtable cycle binding changed");
    return result;
}
} // namespace

CategoryNavigationContract admit_category_navigation_contract(MemoryView memory, const ImageInfo& game) {
    static_assert(sizeof(std::uintptr_t) == 8);
    auto observed = observe(memory, game.range);
    CategoryNavigationContract result;
    result.sites.open = {address(game.range, pins[0].begin, observed.bodies[0].size()),
                         std::move(observed.bodies[0])};
    result.sites.cycle = {address(game.range, pins[1].begin, observed.bodies[1].size()),
                          std::move(observed.bodies[1])};
    result.sites.exact = {address(game.range, pins[6].begin, observed.bodies[6].size()),
                          std::move(observed.bodies[6])};
    result.sites.cycle_caller = address(game.range, 0x6042BD, 1);
    result.sites.back = {address(game.range, 0x60451E, 5),
        {observed.bodies[2].begin() + (0x60451E - pins[2].begin),
         observed.bodies[2].begin() + (0x604523 - pins[2].begin)}};
    result.sites.back_continuation = address(game.range, 0x604B9F, 1);
    result.calls.close = reinterpret_cast<decltype(result.calls.close)>(game.range.base + pins[4].begin);
    result.calls.sound = reinterpret_cast<decltype(result.calls.sound)>(game.range.base + pins[5].begin);
    result.calls.restore_chart = reinterpret_cast<decltype(result.calls.restore_chart)>(game.range.base + pins[7].begin);
    result.calls.set_difficulty = reinterpret_cast<decltype(result.calls.set_difficulty)>(game.range.base + pins[8].begin);
    return result;
}
} // namespace point_zero::native
