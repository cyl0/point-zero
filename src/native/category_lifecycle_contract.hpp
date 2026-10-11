#pragma once

#include "category_lifecycle_hooks.hpp"
#include "image.hpp"
#include <optional>

namespace point_zero::native {
// Finite, read-only LDJ-010-2026081900 / official PL5.1.0 admission. Throws
// on refusal. Inspected images must be retained. Returns
// thirteen complete copied bodies for InlinePatch in CategoryLifeEvent order:
// adopt86FFF0, populate638A70 (or PL+3FEE0), list_destroy639A30, group_destroy611B80,
// initialize603EE0 (or PL+40C60), graph_bind687E20, selection_destroy8EA7E0,
// scene_entry8EB050, scene_tick8EC1F0, scene_refresh8EC750, preview_assign800C70,
// selection_restore607F30, style_apply607D80,
// plus required initialize_caller=game+60717B, scene_refresh_caller=game+8EC3D1,
// preview_caller=game+608A7A, restore_caller=game+8EB6F0,
// style_apply_caller=game+8EC7C1 and style_restore_caller=game+8EC7EF.
// The PL route returns its real callback entry, so before/after encloses BOTH
// native population and PL appending. RCX remains group; preserve native RAX.
// Proves the official incoming route and saved native original, never bypasses it.
// Initialize encloses PL's reset before native navigation/population setup and
// preserves RCX=navigation, EDX=selector and RAX. Only caller607176 is observed;
// caller7FC05B (StepUp-like navigation+258h) continues natively without an observer.
// After original returns, population86E310, style86EC40, working preferences and
// navigation visuals are ready. The enclosing selection initializer607130 still
// restores category/song at6073DC and updates presentation: the scene is NOT done.
// Retained supporting bodies607130/86E310 establish this native ordering.
// Admission checks their bounds/permissions, not whole-body identity.
// Graph binding may guard-return without writing graph+8; its normal completion
// only permits observing the actual saved field. Selection destruction includes
// terminal teardown of its graph at selection+760. Report that identity only
// after normal return, without reading the dead owner. Neither event by itself
// proves enclosing-transaction quiescence or authorizes category destruction.
// Ordinary successful entry and no-message idle tick supply the outer completion
// opportunities. Caller8EB930/97120B ABI is retained static evidence; caller/idle
// extents and normal scene vtable bindings are checked. Collection is gated to that scene,
// caller and idle controller; other scene calls retain an enclosing scope but
// do not authorize cleanup. Entry AL=0/unwinding never permits collection.
// The original tick call must finish its tail continuation before notification.
// Refresh is NativeOne: RCX=scene, preserve native RAX (not a success boolean).
// Guard refresh preparation by exact caller8EC3D1 and admitted scene_vtable. The
// retained complete tick body establishes call8EC3CC and its continuation. Refresh
// captures category/song AFTER entry, then607D80->605520->86EC40 applies style,
// followed by607F30->6060B0 native ID restoration/fallback. Preparation belongs
// before original; refresh completion is not an outer collection boundary.
// Preview assignment is RCX=preview, EDX=int32 song; preserve native RAX when
// forwarded. Complete79B [800C70,800CBF) and independent1403B caller body
// [608850,608DCB), including all five unwind fragments, establish call608A75 and
// return608A7A. The caller passes selection+3A8 (navigation+80) and music+67C's
// song, then overwrites EAX. Native assignment resets preview even for the same
// song. Exact caller alone does not authorize suppression: the callback owns
// entered restore and unchanged-song/preview proof. Unknown callers forward.
// Preview completion is not a collection boundary; engine callees remain external.
// Selection restore adds only the complete199B [607F30,607FF7) entry, including
// its tail JMP. Windows x64: RCX=selection, EDX=id, R8D=song, R9D=p1;
// entry RSP+28h=int p2, +30h=uint8_t forceOpen, +38h=int extra. Preserve all
// seven arguments and native RAX; it is not a success boolean. Exact CALL8EB6EB
// (E8 40 C8 D1 FF), returning8EB6F0, is already owned by scene_entry's complete
// [8EB050,8EB81B) retained extent. No new caller body/read or caller scan is needed.
// Other restore callers retain native forwarding. Restoration/graph/presentation
// helpers remain external to this addition; root/generation policy and enclosing
// transaction completion remain the owner's responsibility.
// Style apply adds the complete14B [607D80,607D8E) leaf: RCX=selection,
// add RCX,328; zero EDX; tail605520. Preserve native RAX through that tail.
// Exact CALL8EC7BC->607D80 and CALL8EC7EA->607F30, returning8EC7C1/8EC7EF,
// are established by retained scene_refresh8EC750..8EC804. The existing entry-restore
// caller8EB6F0 remains independently admitted. Unknown callers still forward;
// the owner/hook decides eligibility and restoration policy within these sites.
// Complete supporting605520..6056C4 (420B),86EC40..86ECF8 (184B), and leaf
// 86F230..86F245 (21B, both RETs) bind finite native style application. Raw
// manager style2 (BATTLE) maps list style0; normal DP raw1 maps1. Native style
// clears open/cursor in exactly460 native categories, not custom backing.
// 605520 directly clears navigation+6, not+5: owner cursor policy stays explicit.
// Apply completion precedes difficulty/ID restoration and is not a collection
// boundary. Virtual list work, engine/restore helpers and EH remain external.
// All four added bodies have no DIR64; only605520/86EC40 have unwind roots,
// neither chained. Their retained extents total639 bytes; runtime does not copy
// the supporting bodies or read their unwind metadata.
// Support86EC40 also admits official PL hk_close_categories40DA0..40E5E,
// objectFE480, saved originalFE4A0: native call40DF2 precedes PL's open/cursor
// reset, preserving native RAX. E9 steals5/T24; FF steals15/T29 with byte14 NOP.
// The same finite forwarding checks apply independently to all three PL routes.
// No extra lifecycle event is installed at the support entry or PL callback.
// Offline current-group routing:605810 writes the active group's style cursor,
// not list+84. Earlier86EC40's ordinary list style virtual may replace list+84
// if the old group is absent from destination IDs; same-group policy is the
// owner's checked condition, not an unconditional native preservation guarantee.
// Close603810 remains admitted by category_navigation_contract. This contract
// also admits complete supporting code [949570,949587), [8854A0,8854A8),
// [897BB0,897BC4), and the direct style-read leaf [9493A0,9493A7), all game RVAs.
// After success, composition assigns existing calls: int style() at949570,
// uintptr_t options() at8854A0, uint8_t battle(uintptr_t options) at897BB0,
// uint8_t keypad(unsigned native_key) at800E70 and int modal() at806F60
// (Windows x64 ABI). No call pointers are added to CategoryLifeSites.
// Raw-style leaf9493A0 loads DWORD game+ACD79A4; options8854A0 loads QWORD
// game+AAB3AA8. Both operand spans must fit game.range.
// This checks bounds only, without copying/querying mutable data or certifying
// its contents, permissions or pointee. BATTLE's helper
// 895A30 (options+224 / logical-player-count decisions) remains external,
// including its count949480 callee. This admits the wrapper, not the full graph
// or transitive BATTLE/filter/toggle behavior. Runtime callers still own context validity.
// Keypad/modal retained evidence covers complete800E70/800F00/A7A160 (all seven unwind
// fragments),806F60/806E60/806E90/806EC0 and leaves806F40/9493E0: nine bodies,
// totaling840 bytes. Admission checks their executable extents. Keypad takes
// the native key NUMBER in ECX and returns
// AL only; native active-side/DP policy is preserved. Modal returns full EAX
// 0/1/-1; only0 permits input. This adds no lifecycle event or hook.
// Bound active DWORDs atACD79B0/B4 and QWORD operands atA7E96D8/A7E9758/A7E9750,
// B28CAD8/B29A250 (minimum image size B29A258), without mutable-data reads or
// permission queries. The input provider, cabinet/type2/map helpers, producers,
// UI343710, virtual/EH targets and their data/pointees remain external; this is
// not transitive input/AFPEngine admission or a proof of input-sample freshness.
// The owner retains tick/context eligibility and one native sample/decision.
// Hook bodies and selected forwarding-chain fields are copied; other support
// function ranges are checked without copying their bodies.
// Only decoded patch/relay instructions require exact bytes.
// No hook, native call, live group/list inspection or game allocation occurs here.
// The caller retains the image and serializes installation/native lifetimes.
// This is not an atomic snapshot or protection against subsequent code edits.
// Supporting layout/cleanup evidence is offline only; external callees, virtual
// targets, EH/CRT machinery and whole-scene readiness are not recursively admitted.
CategoryLifeSites admit_category_lifecycle_contract(
    MemoryView, const ImageInfo& game, const std::optional<ImageInfo>& playlister = std::nullopt);
} // namespace point_zero::native
