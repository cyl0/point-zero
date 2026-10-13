#include "build_bindings.hpp"
#include "category_restore_contract.hpp"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace point_zero::native {
namespace {
using Pin = build_bindings::CategoryRestoreContract::Pin;


// docs/category-filter-refresh-evidence.md supplies the restore route and
// retained body hashes. Independently inspected the final PE as DATA:
// 03_bm2dx/binaries/LDJ-010-2026081900/bm2dx.dll, 18,467,992 bytes,
// SHA256 c61b6dcb8894062e56d60da8ca90053b27f129e1a8e8da5e54457aa42602397d.
// Complete half-open bounds; ALL .pdata CHAININFO descendants were followed.
// Root unwind RVAs and fragment ends (flags2 = handler, not CHAININFO):
//   603BF0: F1D358 flags2; 603C9A.
//   607F30: F1D9E0 flags0; 607FF7 (tail JMP included).
//   6060B0: F1D39C flags2; 6063E8.
//   5FD610: F1AF1C flags0; 5FD637,5FD718,5FD725.
//   608060: F1DAD0 flags0; 60809C,60810F,608117,6081D2,608255,
//           608416,60848B,608490,60849B.
//   6084A0: F1DB70 flags0; 608587,60882B,608843.
//   608850: F1DC50 flags0; 6088A8,6088D9,608DBA,608DBF,608DCB.
//   608DD0: F1DBAC flags0; 608E3D,608E56,608E72,608E7A,609187,
//           609255,60925A,60925F,609269.
// The last THREE complete bodies are required presentation arms beyond the
// documented 608060..60849B dispatcher hash, not fragments of that root.
// Kind0/3 -> 6084A0; kind1 -> 608850; kind2 -> 608DD0. All return to
// 6080D7 and the remaining dispatcher continuation through RET60849A.
// None of these eight spans has an inline/RVA jump table or DIR64 overlap.
// Bar/event dispatch uses compares/branches and immediate event masks.
// Chart mapper82DFA0's six-entry table82E028..82E040 is already admitted by
// sort/filter; no duplicate read here. Unwind/relocation directories establish
// OFFLINE extents, not runtime metadata gates or whole-image fingerprints.
//
// ABI: current takes the navigation argument used by native callers; retained
// 603BF0 actually reloads manager/list globals, resolves index via603E30 then
// 86F0E0(manager,index,3), returning the category pointer. It does not establish
// destination ownership. restore takes RCX=selection, EDX=category, R8D=song,
// R9D=p1; ENTRY RSP+28h=int p2,+30h=byte forceOpen,+38h=int extra.
// 607F30 pushes8/sub40h (48h delta), forwards RCX=selection+328h to6060B0;
// count949480==2 forwards p2, otherwise -1. Selection+D68h panel calls follow,
// then current603BF0 and graph687E20(selection+760h,current). Tail JMP607FF2
// supplies EDX=5/RCX=selection after restoring the frame. RAX is not status.
//
// 6060B0: five pushes/sub70h (98h delta); scans current group by ID, fallback
// ID52 then index0, closes on miss, opens if forceOpen || navigation+5. Clears
// active set1, reloads saved map/rebuilds gated IDs110/210..214, sorts visible
// bars and searches only if song>0. 5FD610 resets cursor0, visits kind1 bars,
// matches song/working difficulty pair and optional chart+34h discriminator;
// every match writes cursor (last duplicate wins). It does not set difficulty.
// In particular, summary8704B0 writes working rows for difficulties0..4 without
// restoring them; 5FD730 restores only category+18h/+1Ch afterward. Native
// metadata1073BC0+40h*id+9 is0 for110/210..214 (unlocked),1 for LEVEL6..17.
// This route alone does NOT preserve a surviving non-highest selected VEFX
// tuple. Parent composition must restore working rows after rebuild and before
// sort/5FD610; these external bodies/data are not added to admission here.
// Header event5 refresh includes mask123Ch graph/highlights; chart event5 uses
// mask224h and song presentation but its graph branch admits only2/9. RANDOM
// graph branch admits only2; event5 still updates its difficulty highlights.
// The wrapper's explicit graph call is therefore required independently.

static_assert([] { std::size_t n{}; for (const auto p : build_bindings::CategoryRestoreContract{}.pins) n += p.end - p.begin; return n; }() == 6064);

void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
std::uintptr_t address(MemoryView memory, ImageRange image, Pin pin) {
    require(image.base >= 0x10000 && contains(image.base, image.size, image.base, image.size) &&
            pin.begin <= image.size && pin.end - pin.begin <= image.size - pin.begin,
            "category restore contract extent outside selected image");
    const auto at = image.base + pin.begin;
    require(readable_range(memory, at, pin.end - pin.begin, RangeUse::image_code, image),
            "category restore contract ownership/protection changed");
    return at;
}
void check_body(MemoryView memory, ImageRange image, Pin pin) {
    const auto at = address(memory, image, pin);
    std::vector<std::byte> bytes(pin.end - pin.begin);
    require(read_bytes(memory, at, bytes), "category restore contract read failed");
    // read_bytes checks readability, not executable image ownership. Recheck
    // the entire range AFTER each copy, when a copy is required.
    (void)address(memory, image, pin);
}
} // namespace

CategoryRestoreCalls admit_category_restore_contract(MemoryView memory, const ImageInfo& game) {
    static_assert(sizeof(std::uintptr_t) == 8);
        for (const auto pin : game_bindings(game.range).category_restore_contract.pins) check_body(memory, game.range, pin);
    CategoryRestoreCalls result;
    result.current = reinterpret_cast<decltype(result.current)>(game.range.base + game_bindings(game.range).category_restore_contract.current_rva);
    result.restore = reinterpret_cast<decltype(result.restore)>(game.range.base + game_bindings(game.range).category_restore_contract.restore_rva);
    return result;
}

// Dependencies, not duplicated graph admission: filter_contract owns5FD730;
// sort_contract owns870D60 and mapper82DFA0/table; lifecycle owns687E20;
// navigation owns open603900/close603810/input604110. Their hooks/originals
// must be composed by the parent. Unknown edits in OUR selected incoming
// bodies refuse (no trampoline following, patch masking or compatibility skip).
// All other transitive engine/virtual/map/accessor/panel/CRT/EH implementations
// remain external, including603E30/86F0E0/605810,6076C0/607D90,5FC440,
// 949480, saved/active-map operations and mutable global/list/chart storage.
} // namespace point_zero::native
