#include "build_bindings.hpp"
#include "category_lifecycle_contract.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <span>
#include <utility>
#include <stdexcept>
#include <vector>

namespace point_zero::native {
namespace {
using Pin = build_bindings::CategoryLifecycleContract::Pin;

// Retained final 03_bm2dx/binaries/LDJ-010-2026081900/bm2dx.dll SHA256:
// c61b6dcb8894062e56d60da8ca90053b27f129e1a8e8da5e54457aa42602397d.
// Complete half-open .pdata extents, including populate/group error tails.
// UW respectively FB68C8 (v1 flags0), F2DCC8 (v1 flags0), F2DE30 (v1 flags3),
// F1FBFC (v1 flags3). Flags3 means handler, not CHAININFO. Initialize has three
// contiguous fragments:603EE0..603F4F UW F1D06C flags0;603F4F..603FAC UW F1D07C
// flags4;603FAC..604109 UW F1D090 flags4. Both chains name the first fragment.
// Retained initializer union is553 bytes. No selected instruction
// or literal overlaps a DIR64; there are no omitted chained descendants.
// Independent tests traverse the full PE exception/relocation directories;
// Runtime admission copies hook/official PL chain slices and checks support
// extents/permissions; it does not read the PE exception/relocation directories.
// Graph's six contiguous fragments cover687E20..687F53, rootUW F45BB4;
// selection destruction is8EA7E0..8EA9E2, UW FDA5F8. See the retained graph and
// selection-retirement evidence for exact saved-field/lifetime provenance.
// Refresh8EC750..8EC804 has one complete root, UW FDABF8 flags0, no chains
// or DIR64. Caller8EC3CC/return8EC3D1 lies inside the existing scene_tick pin.
// Preview800C70..800CBF is79B, UW FA1FF8 v1 flags0, no descendants/DIR64.
// Restore607F30..607FF7 is199B, UW F1D9E0 v1 flags0, no descendants/DIR64.
// Its complete tail JMP607FF2 is included. CALL8EB6EB (E8 40 C8 D1 FF) and
// return8EB6F0 are inside the admitted scene_entry8EB050..8EB81B body.
// Style apply607D80..607D8E is a complete14B leaf tail wrapper, no .pdata/DIR64.
// Its CALL8EC7BC (E8 BF B5 D1 FF), return8EC7C1, and restore CALL8EC7EA
// (E8 41 B7 D1 FF), return8EC7EF, are inside the existing scene_refresh pin.

// Complete supporting bodies, no DIR64. Only608850/A7A160 have chained descendants,
// all included below. First five UW:
// F1D938/FB6918/FDA6F8/FF7F84/FDA900. Callees/handler metadata remain external.

// Entrance: two complete leaves, no unwind records or DIR64. Getter returns
// inline game+A7E90A8 (not a pointer slot); reset clears seven DWORDs through+18.
// No callees, selection/style writes or preview work. Checked at preparation,
// not on each toggle/frame. Existing larger global bounds include this state.
// BATTLE adds639B/pass: wrapper14B plus three supporting bodies625B.
// 605520..6056C4 is420B, UW F1D0A0 v1 flags2 (UHANDLER, not CHAININFO);
// 86EC40..86ECF8 is184B, UW FB6C4C v1 flags0. Neither has descendants.
// 86F230..86F245 is a21B leaf with BOTH RETs, no .pdata. No new body has DIR64.
// Wrapper: RCX=selection -> navigation+328, EDX=0, tail605520; preserve RAX.
// 60554E calls86EC40(manager,4) before list/group work;60569A clears nav+6,
// NOT nav+5. The complete body includes tail6056BF->7FB460. Callee side effects
// remain external: do not infer that this resets a custom navigation cursor.
// 86EC40 chooses raw manager+2A424=2 for requested DP plus BATTLE, otherwise
// normalizes requested style to0/1. Its two230-slot loops clear native category
// +4 open byte and +14 cursor DWORD (manager+10, stride D0): exactly460 slots.
// This is not a custom-category sweep. For a nonnull manager+175D0 list,
// 86ECD7->86F230 returns1 ONLY for raw1; raw2 maps0, then virtual list+10 gets
// EDX=normalized style. Its target and downstream population remain external.
// Exact apply/restore callers bind the existing refresh sequence; style finish
// is before working-difficulty and ID restoration, never outer quiescence.
// See docs/category-battle-evidence.md for bounded offline chain/branch evidence.
// Preview caller608850..608DCB is1403B. Complete contiguous .pdata fragments:
// 608850..6088A8 UW F1DC50 flags0;6088A8..6088D9 UW F1DC60 flags4 -> root;
// 6088D9..608DBA UW F1DC74 flags4 ->6088A8;608DBA..608DBF UW F1DC90 flags4
// ->6088A8;608DBF..608DCB UW F1DCA0 flags4 -> root (all v1). Independently
// pinned here even when category_restore_contract has admitted the same body.
// Together the preview entry/caller add1482 bytes in retained evidence, no engine closure.
// These four refresh-call bodies add exactly58 bytes in retained evidence. Requested
// style949570 is23B, UW FF0AC4 v1 flags0: call949574->9493A0, normalize EAX to
// 0/1. Its7B leaf9493A0 loads DWORD game+ACD79A4. Options8854A0 is an8B leaf
// loading QWORD game+AAB3AA8 into RAX. Both leaves end in RET, have no .pdata
// record/descendant, and reference mutable .data through fixed RIP displacements.
// BATTLE897BB0 is20B, UW FC0014 v1 flags0: preserve RCX=options, call895A30,
// return AL=(EAX==1); upper EAX bits are not a boolean return contract. Offline
// helper895A30..895A6D (UW FC000C flags0) retains options and conditionally reads
// DWORD options+224 after native logical-player count949480/style949570 checks.
// The helper/count bodies and mutable global/owner contents remain external;
// only the four listed code bodies are added, not a recursive BATTLE contract.
// Composition may assign the three public call RVAs after successful admission.
//
// Keypad/modal add exactly nine bodies/840B in retained evidence.800E70 is90B,
// UW FA206C;800F00 is98B, UW FA2060;806F60 is81B, UW FA3370. Modal wrappers
// 806E60/806E90/806EC0 are44B each, UW FA3378/FA3388/FA3380 respectively.
// All six roots are v1 flags0 without descendants.806F40 is an18B leaf and
// 9493E0 a30B leaf: BOTH branch returns are included, no .pdata overlaps.
// A7A160 is391B, all seven v1 fragments: A7A160..A7A1B6 UW1031758 flags0;
// A7A1B6..A7A1CD UW1031764 flags4 -> root; A7A1CD..A7A21B UW1031778 flags4
// ->A7A1B6; A7A21B..A7A290 UW103178C flags4 ->A7A1CD; A7A290..A7A295
// UW10317A0 flags4 ->A7A1CD; A7A295..A7A29A UW10317B0 flags4 ->A7A1B6;
// A7A29A..A7A2E7 UW10317C0 flags4 -> root. No selected body has DIR64.
//
// After admission composition assigns uint8_t keypad(unsigned native_key) at
// 800E70 and int modal() at806F60 (Windows x64). No new site/callback/pointer
// abstraction. Keypad takes ECX=the native key number, not a bit mask; only AL
// is its boolean result. It retains key in EDI, asks9493E0 about physical sides
// 0/1 and ORs their800F00 results.9493E0 guards index0..1 then reads the two
// DWORD active flags at ACD79B0/B4.800F00 passes R8D=key, EDX=side and the
// external A79B50 result in RCX to A7A160. On false, admitted style949570 may
// select the opposite physical side for DP. Preserve this native input policy.
// A7A160 retains key/side in R14D/R15, loads the map QWORD B29A250 and optional
// owner QWORD B28CAD8. Type3 compares mapped/registered keys' DWORD+60 to1,
// ORing results; shared-reference releases do not clear the input fields.
// Type2 dispatches94BEF0/94D150 and ORs AL. Input-provider A79B50, cabinet93B9D0,
// map/key access9DA5F0/9C8CF0, type2 helpers, producers and virtual destroy
// targets remain external. These body pins do not prove producer freshness,
// global exactly-once input or the transitive input/AFPEngine graph.
//
// Modal wrappers load QWORDs A7E96D8/A7E9758/A7E9750 respectively, return0
// for null, otherwise pass external343710's EAX as ECX and EDX=10 to806F40.
// Its signed comparison maps state>=10 to-1; below10 maps zero to0/nonzero to1.
// 806F60 combines these states, returning0 only if all are0, -1 if any is-1,
// otherwise1. This is a full EAX result; caller must require exactly0.
// Operand spans are bounded below, not read/permission-queried: no mutable
// value or pointee certification. New minimum game.range.size is B29A258.
// Owner tick eligibility, modal timing and one sample/decision per outer tick
// remain parent responsibilities; admission performs no native query/polling.

constexpr Pin pl_populate{0x3FEE0, 0x40750};
constexpr std::array<unsigned char, 14> populate_original{
    0x48,0x89,0x5C,0x24,0x10,0x48,0x89,0x6C,0x24,0x18,0x56,0x57,0x41,0x54};
// PL also hooks603EE0, discovered by signature at603EEB. Installer42161 calls
// 42650;4265D binds40C60;42169 stores hook objectFE690. Callback40C60..40D96,
// UW F80DC v1 flags2, no chains/DIR64. It initializes each category via100828,
// clears playlist inserted and chart_count, THEN calls saved originalFE6B0 at
// 40D4A with unchanged navigation/selector; RAX survives mutex release. Hooking
// the callback preserves both resets and original game return address60717B.
// Fields FE690/+8, FE6B0/+8, FE6D8 mirror populate; mutexFE6E0 remains external.
// E9 steals5, T24. FF steals15 (5+5+1+4), incoming byte14 MUST be NOP, T29:
// original[15], FF25+game+15. Populate remains stolen14/T28. Native prefixes are
// nonrelative. These event routes and the supporting close route are independent.
constexpr Pin pl_initialize{0x40C60, 0x40D96};
constexpr std::array<unsigned char, 15> initialize_original{
    0x48,0x89,0x5C,0x24,0x18,0x48,0x89,0x6C,0x24,0x20,0x57,0x48,0x83,0xEC,0x30};
// Official hk_close_categories overlaps support86EC40: preserve PL->native,
// then PL's custom open/cursor reset. Complete190B callback40DA0..40E5E,
// UW F8114 v1 flags2, no descendants/DIR64. Installer42138->42620 binds
// callback at4262D and objectFE480 at42140. Saved originalFE4A0 is called at
// 40DF2 with unchanged manager/selector;40DF4 saves RAX,40E32 restores it.
// Its mutexFE4D0, playlist bounds1008F8/100900, EH/CRT remain external.
// Native prefix5+5+1+4 is nonrelative: E9 steals5/T24; FF steals15/T29 with
// incoming byte14 NOP. This support route adds no lifecycle hook/site.
constexpr Pin pl_close{0x40DA0, 0x40E5E};
constexpr std::array<unsigned char, 15> close_original{
    0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x20};
constexpr std::array<unsigned char, 6> indirect{0xFF,0x25,0,0,0,0};

void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
template<class T> T value(const std::vector<std::byte>& bytes, std::size_t offset) {
    require(offset <= bytes.size() && sizeof(T) <= bytes.size() - offset,
            "category lifecycle field outside finite observation");
    T result{};
    std::memcpy(&result, bytes.data() + offset, sizeof(result));
    return result;
}
bool prefix(const std::vector<std::byte>& bytes, std::size_t offset,
            std::span<const unsigned char> expected, std::size_t size = std::dynamic_extent) {
    if (size == std::dynamic_extent) size = expected.size();
    return size <= expected.size() && offset <= bytes.size() && size <= bytes.size() - offset &&
        std::equal(expected.begin(), expected.begin() + size, bytes.begin() + offset,
                   [](unsigned char a, std::byte b) { return a == std::to_integer<unsigned char>(b); });
}
std::uintptr_t address(ImageRange image, std::uint32_t rva, std::size_t size) {
    require(image.base >= 0x10000 && contains(image.base, image.size, image.base, image.size) &&
            rva <= image.size && size <= image.size - rva,
            "category lifecycle contract extent outside selected image");
    return image.base + rva;
}
std::vector<std::byte> copy_image(MemoryView memory, ImageRange image, std::uint32_t rva,
                                  std::size_t size, RangeUse use) {
    const auto at = address(image, rva, size);
    require(readable_range(memory, at, size, use, image),
            "category lifecycle contract image ownership/protection changed");
    std::vector<std::byte> bytes(size);
    require(read_bytes(memory, at, bytes), "category lifecycle contract read failed");
    require(readable_range(memory, at, size, use, image),
            "category lifecycle contract image changed during read");
    return bytes;
}
std::uintptr_t relative(std::uintptr_t instruction, std::int32_t displacement) {
    require(instruction <= (std::numeric_limits<std::uintptr_t>::max)() - 5,
            "category lifecycle relative instruction overflow");
    const auto next = instruction + 5;
    const auto delta = static_cast<std::int64_t>(displacement);
    if (delta < 0) {
        require(next >= static_cast<std::uint64_t>(-delta), "category lifecycle relative target underflow");
        return next - static_cast<std::uintptr_t>(-delta);
    }
    require(static_cast<std::uint64_t>(delta) <= (std::numeric_limits<std::uintptr_t>::max)() - next,
            "category lifecycle relative target overflow");
    return next + static_cast<std::uintptr_t>(delta);
}
void private_range(MemoryView memory, std::uintptr_t at, std::size_t size, std::uintptr_t allocation) {
    require(readable_range(memory, at, size, RangeUse::private_code),
            "category lifecycle original is not readable private executable memory");
    const auto end = at + size;
    for (auto cursor = at; cursor < end;) {
        MemoryRegion region{};
        require(memory.query && memory.query(memory.context, cursor, region) &&
                contains(region.begin, region.size, cursor, 1) && region.allocation == allocation &&
                region.kind == MemoryKind::private_memory && region.committed && region.readable &&
                region.executable && !region.guarded,
                "category lifecycle original allocation changed");
        cursor = (std::min)(end, region.begin + region.size);
    }
}
struct Forwarding {
    std::vector<std::byte> incoming, binding, saved, extent, trampoline;
    std::uintptr_t allocation{};
};
struct Observation {
    CategoryLifeSites sites;
    std::array<Forwarding, 3> forwarding;
};
CategoryLifeSite forwarded(MemoryView memory, const ImageInfo& game,
                           const std::optional<ImageInfo>& pl, unsigned route, Forwarding& observation) {
    // Finite route index: populate0, initialize1, supporting close2.
    const std::array native_pins{game_bindings(game.range).category_lifecycle_contract.pins[1], game_bindings(game.range).category_lifecycle_contract.pins[4], game_bindings(game.range).category_lifecycle_contract.support_pins[20]};
    constexpr std::array callbacks{pl_populate, pl_initialize, pl_close};
    constexpr std::array bindings{0xFDEA0U, 0xFE690U, 0xFE480U};
    const std::array<std::span<const unsigned char>, 3> prefixes{
        populate_original, initialize_original, close_original};
    require(route < native_pins.size(), "category lifecycle invalid finite forwarding route");
    const auto pin = native_pins[route];
    const auto callback = callbacks[route];
    const auto native_prefix = prefixes[route];
    const auto binding = bindings[route];
    auto& incoming = observation.incoming;
    incoming = copy_image(memory, game.range, pin.begin, pin.end - pin.begin, RangeUse::image_code);
    const auto native = address(game.range, pin.begin, incoming.size());
    if (prefix(incoming, 0, native_prefix, 5)) {
        return {native, incoming};
    }
    require(pl.has_value(), "category lifecycle entry patched without selected Playlister image");
    const bool near = incoming[0] == std::byte{0xE9};
    const bool far = prefix(incoming, 0, indirect);
    require(near || far, "category lifecycle unsupported incoming patch shape");
    const auto stolen = near ? std::size_t{5} : native_prefix.size();
    const auto size = near ? std::size_t{24} : stolen + 14;
    require(!far || stolen == 14 || incoming[14] == std::byte{0x90},
            "category lifecycle 15-byte FF padding changed");
    const auto implementation = address(pl->range, callback.begin, callback.end - callback.begin);
    require(game.range.base + game.range.size <= pl->range.base ||
            pl->range.base + pl->range.size <= game.range.base,
            "category lifecycle game and Playlister images overlap");
    CategoryLifeSite site{implementation, copy_image(memory, pl->range, callback.begin,
                          callback.end - callback.begin, RangeUse::image_code)};
    observation.binding = copy_image(memory, pl->range, binding, 16, RangeUse::image_read);
    observation.saved = copy_image(memory, pl->range, binding + 0x20, 16, RangeUse::image_read);
    observation.extent = copy_image(memory, pl->range, binding + 0x48, 8, RangeUse::image_read);
    require(value<std::uintptr_t>(observation.binding, 0) == native &&
            value<std::uintptr_t>(observation.binding, 8) == implementation,
            "category lifecycle Playlister target/destination changed");
    const auto trampoline = value<std::uintptr_t>(observation.saved, 0);
    require(value<std::uint64_t>(observation.saved, 8) == size &&
            value<std::uint64_t>(observation.extent, 0) == size &&
            contains(trampoline, size, trampoline, size),
            "category lifecycle Playlister original extent changed");
    require(near ? relative(native, value<std::int32_t>(incoming, 1)) == trampoline + 10 :
                   value<std::uintptr_t>(incoming, 6) == implementation,
            "category lifecycle incoming route disagrees with original/destination");
    MemoryRegion region{};
    require(memory.query && memory.query(memory.context, trampoline, region),
            "category lifecycle original allocation unavailable");
    observation.allocation = region.allocation;
    private_range(memory, trampoline, size, observation.allocation);
    observation.trampoline.resize(size);
    require(read_bytes(memory, trampoline, observation.trampoline), "category lifecycle original read failed");
    private_range(memory, trampoline, size, observation.allocation);
    const auto& original = observation.trampoline;
    require(prefix(original, 0, native_prefix, stolen), "category lifecycle stolen native prefix changed");
    if (near) {
        require(original[5] == std::byte{0xE9} &&
                relative(trampoline + 5, value<std::int32_t>(original, 6)) == native + 5 &&
                prefix(original, 10, indirect) && value<std::uintptr_t>(original, 16) == implementation,
                "category lifecycle E9 original/relay changed");
    } else {
        require(prefix(original, stolen, indirect) && value<std::uintptr_t>(original, stolen + 6) == native + stolen,
                "category lifecycle FF original continuation changed");
    }
    return site;
}
Observation observe(MemoryView memory, const ImageInfo& game, const std::optional<ImageInfo>& pl) {
    Observation result{};
    for (std::size_t i = 0; i < build_bindings::CategoryLifecycleContract{}.pins.size(); ++i) {
        if (i == 1 || i == 4)
            result.sites.entries[i] = forwarded(memory, game, pl, i == 4 ? 1U : 0U, result.forwarding[i == 4 ? 1 : 0]);
        else {
            const auto pin = game_bindings(game.range).category_lifecycle_contract.pins[i];
            auto body = copy_image(memory, game.range, pin.begin, pin.end - pin.begin, RangeUse::image_code);
            result.sites.entries[i] = {address(game.range, pin.begin, body.size()), std::move(body)};
        }
    }
    for (std::size_t i = 0; i < game_bindings(game.range).category_lifecycle_contract.support_pins.size(); ++i) {
        const auto pin = game_bindings(game.range).category_lifecycle_contract.support_pins[i];
        if (pin.begin == game_bindings(game.range).category_lifecycle_contract.category_lookup_rva) {
            // Observe the real incoming PL close chain; never require stock
            // bytes at the patched entry or bypass its post-native reset.
            (void)forwarded(memory, game, pl, 2, result.forwarding[2]);
            continue;
        }
        const auto at = address(game.range, pin.begin, pin.end - pin.begin);
        require(readable_range(memory, at, pin.end - pin.begin, RangeUse::image_code, game.range),
                "category lifecycle support ownership/protection changed");
    }
    // The retained getters' complete RIP-relative data operands must fit the
    // claimed image. Bounds only: no mutable-data copy or permission query.
    (void)address(game.range, game_bindings(game.range).category_lifecycle_contract.category_state_rva, 8);
    (void)address(game.range, game_bindings(game.range).category_lifecycle_contract.dp_flag_rva, 4);
    (void)address(game.range, game_bindings(game.range).category_lifecycle_contract.active_flags_rva, 8); // Two guarded DWORD active-side flags.
    (void)address(game.range, game_bindings(game.range).category_lifecycle_contract.controller_slot_1_rva, 8);
    (void)address(game.range, game_bindings(game.range).category_lifecycle_contract.controller_slot_2_rva, 8);
    (void)address(game.range, game_bindings(game.range).category_lifecycle_contract.controller_slot_3_rva, 8);
    (void)address(game.range, game_bindings(game.range).category_lifecycle_contract.scene_slot_1_rva, 8);
    (void)address(game.range, game_bindings(game.range).category_lifecycle_contract.scene_slot_2_rva, 8);
    result.sites.initialize_caller = address(game.range, game_bindings(game.range).category_lifecycle_contract.sites_initialize_caller_rva, 1);
    const std::array slots{game_bindings(game.range).category_lifecycle_contract.scene_entry_slot_rva, game_bindings(game.range).category_lifecycle_contract.scene_refresh_slot_rva}, targets{game_bindings(game.range).category_lifecycle_contract.scene_entry_rva, game_bindings(game.range).category_lifecycle_contract.scene_refresh_rva};
    for (std::size_t i = 0; i < slots.size(); ++i) {
        const auto binding = copy_image(memory, game.range, slots[i], 8, RangeUse::image_read);
        require(value<std::uintptr_t>(binding, 0) == address(game.range, targets[i], 1),
                "category lifecycle ordinary scene vtable changed");
    }
    result.sites.scene_entry_caller = address(game.range, game_bindings(game.range).category_lifecycle_contract.sites_scene_entry_caller_rva, 1);
    result.sites.scene_tick_caller = address(game.range, game_bindings(game.range).category_lifecycle_contract.sites_scene_tick_caller_rva, 1);
    result.sites.scene_refresh_caller = address(game.range, game_bindings(game.range).category_lifecycle_contract.sites_scene_refresh_caller_rva, 1);
    result.sites.preview_caller = address(game.range, game_bindings(game.range).category_lifecycle_contract.sites_preview_caller_rva, 1);
    result.sites.restore_caller = address(game.range, game_bindings(game.range).category_lifecycle_contract.sites_restore_caller_rva, 1);
    result.sites.style_apply_caller = address(game.range, game_bindings(game.range).category_lifecycle_contract.sites_style_apply_caller_rva, 1);
    result.sites.style_restore_caller = address(game.range, game_bindings(game.range).category_lifecycle_contract.sites_style_restore_caller_rva, 1);
    result.sites.scene_vtable = address(game.range, game_bindings(game.range).category_lifecycle_contract.scene_vtable_rva, 0x80);
    result.sites.idle_callback = address(game.range, game_bindings(game.range).category_lifecycle_contract.idle_callback_rva, 1);
    return result;
}
} // namespace

CategoryLifeSites admit_category_lifecycle_contract(
    MemoryView memory, const ImageInfo& game, const std::optional<ImageInfo>& playlister) {
    static_assert(sizeof(std::uintptr_t) == 8);
    static_assert(static_cast<unsigned>(CategoryLifeEvent::adopt) == 0 &&
                  static_cast<unsigned>(CategoryLifeEvent::populate) == 1 &&
                  static_cast<unsigned>(CategoryLifeEvent::list_destroy) == 2 &&
                  static_cast<unsigned>(CategoryLifeEvent::group_destroy) == 3 &&
                  static_cast<unsigned>(CategoryLifeEvent::initialize) == 4 &&
                  static_cast<unsigned>(CategoryLifeEvent::graph_bind) == 5 &&
                  static_cast<unsigned>(CategoryLifeEvent::selection_destroy) == 6 &&
                  static_cast<unsigned>(CategoryLifeEvent::scene_entry) == 7 &&
                  static_cast<unsigned>(CategoryLifeEvent::scene_tick) == 8 &&
                  static_cast<unsigned>(CategoryLifeEvent::scene_refresh) == 9 &&
                  static_cast<unsigned>(CategoryLifeEvent::preview_assign) == 10 &&
                  static_cast<unsigned>(CategoryLifeEvent::selection_restore) == 11 &&
                  static_cast<unsigned>(CategoryLifeEvent::style_apply) == 12);
    static_assert(std::tuple_size_v<decltype(CategoryLifeSites::entries)> == build_bindings::CategoryLifecycleContract{}.pins.size());
    auto observed = observe(memory, game, playlister);
    return std::move(observed.sites);
}
} // namespace point_zero::native
