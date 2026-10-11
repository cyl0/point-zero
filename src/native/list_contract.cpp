#include "list_contract.hpp"
#include "image_metadata.hpp"

#include <stdexcept>

namespace point_zero::native {
namespace {
struct Pin { std::uint32_t rva, size; };
struct Entry { std::uint16_t ordinal; Pin code; };

// Retained LDJ-010-2026081900 DLLs, read as PE data. Complete per-function
// spans, with no base relocations inside any pin. Emit's seven chained unwind
// fragments end at 34B38D,34B398,34B569,34B5FF,34B62D,34B6A6,34B6B8:
// its first return/root unwind fragment is NOT the complete function.
// Leaf pins stop before padding/next function and include every return path.
constexpr Pin game_code[]{
    {0x949480, 55},
    {0x949530, 57},
    {0x949230, 42},
    {0x34B360, 856},
    {0x34C990, 186},
    {0x34C240, 19},
    {0x353A90, 82},
    {0x34C0F0, 112},
    {0x34C3B0, 156},
    {0x34C450, 119},
    {0x34C720, 27},
};

// Only exports called by ListNativeCalls or the selected emission wrappers;
// unrelated cup path/attach/control/animation-info functions are not required.
constexpr Entry core_exports[]{
    {20, {0x4410, 61}},
    {21, {0x4450, 8}},
    {22, {0x37B0, 116}},
    {35, {0xB7D0, 85}},
    {37, {0xB930, 242}},
    {45, {0x5250, 195}},
    {46, {0x53D0, 130}},
    {47, {0x5610, 66}},
    {49, {0x5660, 110}},
    {50, {0x56D0, 110}},
    {51, {0x5740, 76}},
    {52, {0x5830, 48}},
    {56, {0xC5B0, 74}},
    {85, {0x19590, 226}},
    {106, {0x1DA50, 416}},
    {116, {0x22D60, 238}},
};

constexpr Entry utils_exports[]{
    {56, {0xB980, 5}},
    {71, {0xDC10, 60}},
    {77, {0xDCD0, 79}},
};

// Emitted properties 1003/1006/101B/101E, native emitter scale helper,
// relative(6)'s branch and relative tail target, and stream flag mutation.
constexpr Pin core_helpers[]{
    {0x3E950, 167},
    {0x228C0, 106},
    {0x22970, 16},
    {0x22B20, 62},
    {0x22BA0, 25},
    {0x36E20, 87},
    {0x1D8D0, 26},
    {0x1D830, 159},
    {0xA770, 315},
};

// Package's B980 tail goes to B950; image-info's DC47 tail goes to DA30.
// DA30 includes the early -2 return AND the success path through DB1B.
constexpr Pin utils_helpers[]{
    {0xB950, 41},
    {0xDA30, 236},
};

struct Binding { std::uint32_t slot, target; };
constexpr Binding game_tables[]{
    {0xE6C838, 0x34C240}, // visible, vtable +28
    {0xE6C860, 0x34C3B0}, // emit's +50 matrix initialization
    {0xE6C878, 0x34C450}, // emit's +68 pivot
    {0xE6C898, 0x34C720}, // wrappers compare the +88 flag method
    {0xE6C8E0, 0x34C990}, // emit's +D0 scale
    {0xE6C900, 0x34CAD0}, // initializer compares +F0 before inlining it
};
constexpr Binding core_tables[]{
    {0xDD390 + 8 * 3,  0x228C0}, // property(1003)
    {0xDD390 + 8 * 6,  0x22970}, // property(1006)
    {0xDD390 + 8 * 27, 0x22B20}, // property(101B)
    {0xDD390 + 8 * 30, 0x22BA0}, // property(101E)
};
struct Import { std::uint32_t slot; std::uint16_t ordinal; };
constexpr Import game_imports[]{
    {0xC91B50,56}, {0xC91B88,106}, {0xC91BC8,116},
    {0xC91C40,35}, {0xC91C48,37}, {0xC91C68,45},
    {0xC91C80,47}, {0xC91C88,51}, {0xC91C90,49},
    {0xC91C98,50}, {0xC91CA0,46}, {0xC91CB0,52}, {0xC91CB8,85},
};

void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
std::uintptr_t address(MemoryView memory, ImageRange image, std::size_t rva,
                       std::size_t size, RangeUse use = RangeUse::image_read) {
    require(contains(image.base, image.size, image.base, image.size) &&
            rva <= image.size && size <= image.size - rva,
            "list contract range outside selected image");
    const auto at = image.base + rva;
    require(readable_range(memory, at, size, use, image),
            "list contract allocation or protection changed");
    return at;
}
template<class T> T field(MemoryView memory, ImageRange image, std::size_t rva) {
    T value{};
    require(read_value(memory, address(memory, image, rva, sizeof(T)), value),
            "list contract read failed");
    return value;
}
void code(MemoryView memory, ImageRange image, Pin pin) {
    (void)address(memory, image, pin.rva, pin.size, RangeUse::image_code);
}
void exported(MemoryView memory, const ImageInfo& image, const Entry& entry) {
    const auto at = address(memory, image.range, entry.code.rva, 1, RangeUse::image_code);
    detail::MetadataView metadata{memory, image.range};
    const auto target = inspect_export(metadata.view(), image, entry.ordinal);
    require(target.forwarder.empty() && target.address == at, "list export target changed");
}
void table(MemoryView memory, ImageRange image, Binding binding) {
    require(field<std::uintptr_t>(memory, image, binding.slot) ==
            address(memory, image, binding.target, 1, RangeUse::image_code),
            "list dispatch target changed");
}
void bindings(MemoryView memory, const NativeModules& modules) {
    for (const auto& entry : core_exports) exported(memory, modules.afp, entry);
    for (const auto& entry : utils_exports) exported(memory, modules.utils, entry);
    for (const auto binding : game_tables) table(memory, modules.game.range, binding);
    for (const auto binding : core_tables) table(memory, modules.afp.range, binding);
    for (const auto binding : game_imports) {
        const auto imported = inspect_import(memory, modules.game, binding.slot);
        const Entry* selected{};
        for (const auto& entry : core_exports)
            if (entry.ordinal == binding.ordinal) { selected = &entry; break; }
        require(selected && imported.provider == "afp-core.dll" &&
                imported.ordinal == binding.ordinal && imported.name.empty() &&
                imported.target == modules.afp.range.base + selected->code.rva,
                "list game AFP import provider, ordinal or target changed");
    }
}
} // namespace

// Finite call admission, not recursive native implementation admission. The
// exact tables above cover selected sprite virtual calls and AFP properties;
// deeper relative callees (including chart helpers), allocator/stream lookup
// internals, utils imports, scalar constants/global state and AVS logging are
// outside this contract. Captured chart identities are checked separately.
//
// List names/compact names tables and list-draw/title hook sites, forwarding,
// patch ownership, live chart/sprite/pool/package layouts and menu-thread
// lifetime/gating remain SEPARATE contracts. This does not admit those hooks,
// execute native code, load a DLL, or certify a future mutable-memory snapshot.
ListNativeCalls admit_list_calls(MemoryView memory, const NativeModules& modules) {
    bindings(memory, modules);
    for (const auto pin : game_code) code(memory, modules.game.range, pin);
    for (const auto& entry : core_exports) code(memory, modules.afp.range, entry.code);
    for (const auto& entry : utils_exports) code(memory, modules.utils.range, entry.code);
    for (const auto pin : core_helpers) code(memory, modules.afp.range, pin);
    for (const auto pin : utils_helpers) code(memory, modules.utils.range, pin);
    const auto g = modules.game.range.base, c = modules.afp.range.base, u = modules.utils.range.base;
    ListNativeCalls result;
    result.chart.player_count = reinterpret_cast<decltype(result.chart.player_count)>(g + 0x949480);
    result.chart.style = reinterpret_cast<decltype(result.chart.style)>(g + 0x949530);
    result.chart.active_player = reinterpret_cast<decltype(result.chart.active_player)>(g + 0x949230);
    result.valid = reinterpret_cast<decltype(result.valid)>(c + 0x37B0);
    result.package = reinterpret_cast<decltype(result.package)>(u + 0xB980);
    result.image_info = reinterpret_cast<decltype(result.image_info)>(u + 0xDC10);
    result.arguments = reinterpret_cast<decltype(result.arguments)>(u + 0xDCD0);
    result.creation_level = reinterpret_cast<decltype(result.creation_level)>(c + 0x4450);
    result.set_creation_level = reinterpret_cast<decltype(result.set_creation_level)>(c + 0x4410);
    result.emit = reinterpret_cast<decltype(result.emit)>(g + 0x34B360);
    // 34C240 consumes DL (use 0/1), passes stream,[mask=1],value, then
    // tail-jumps via C91B50 to core ordinal 56 / XCd229cc000037 / C5B0.
    // EAX is preserved: C5B0 returns 0 after A770, or -3 on lookup failure.
    // The retained probe discarded that result; the typed int is intentional.
    result.visible = reinterpret_cast<decltype(result.visible)>(g + 0x34C240);
    return result;
}
} // namespace point_zero::native
