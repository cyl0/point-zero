#include "build_bindings.hpp"
#include "played_calls_contract.hpp"
#include "image_metadata.hpp"
#include "cup_contract.hpp"

#include <windows.h>
#include <stdexcept>

namespace point_zero::native {
namespace {
using Import = build_bindings::PlayedCallsContract::Import;
using Pin = build_bindings::PlayedCallsContract::Pin;

struct Entry { std::uint16_t ordinal; std::uint32_t rva; };

// Derived from the retained final bm2dx/afp-core/afp-utils PE files as data.
// No pin intersects a base relocation. These are retained executable extents,
// not April prefixes, the first RET or just the first RUNTIME_FUNCTION record.
// Core chained unwind fragments (half-open ends, contiguous from the root):
// 1D350: 1D387,1D3E0,1D467,1D490
// 1DF60: 1DFAF,1E020,1E026,1E144,1E17F
// 1EA70: 1EAB2,1EB06,1EB1E,1EB91,1EBE9,1EC1D
// 22740: 22772,227B0,227CC
// 521A0: 521CD,52201,522D5,522FA
// The remaining unwind-backed pins have no chained descendants; leaf spans
// include every branch through the final RET/tail JMP, excluding INT3 padding.
// Unwind metadata derives these bounds; it is not itself admitted for execution.

constexpr Pin core_code[]{
    {0x1EA70, 429},
    {0x48AC0, 56},
    {0x521A0, 346},
    {0x41A0, 160},
    {0x4260, 41},
    {0x22740, 140},
    {0x3BB10, 82},
    // Stream selection/restore tail targets, relative(6)'s tail path and
    // bitmap-argument conversion directly delegated by the selected binder.
    {0x40F0, 41},
    {0x4120, 121},
    {0x1D8D0, 26},
    {0x1D830, 159},
    {0x1E850, 81},
};
constexpr Pin utils_code[]{
    {0xDC10, 60},
    {0xDCD0, 79},
    // Image/animation info tail targets, including their early null returns.
    {0xDA30, 236},
    {0x108C0, 49},
};

constexpr Entry core_exports[]{ {137, 0x1EA70} };
constexpr Entry utils_exports[]{
    {71, 0xDC10},
    {77, 0xDCD0},
};
// The ordinary native named wrapper at 340390 uses these two imports after
// stock bitmap forwarding. Its code is admitted by admit_played_sites. The
// separately hooked stock bitmap slot C91BD8 is owned by SurfaceHooks, not us.



void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
std::uintptr_t address(MemoryView memory, ImageRange image, std::size_t rva,
                       std::size_t size, RangeUse use = RangeUse::image_read) {
    require(image.base >= 0x10000 && contains(image.base, image.size, image.base, image.size) &&
            rva <= image.size && size <= image.size - rva,
            "played calls range outside selected image");
    const auto at = image.base + rva;
    require(readable_range(memory, at, size, use, image),
            "played calls allocation or protection changed");
    return at;
}
template<class T> T field(MemoryView memory, ImageRange image, std::size_t rva) {
    T result{};
    require(read_value(memory, address(memory, image, rva, sizeof(T)), result),
            "played calls read failed");
    return result;
}
void code(MemoryView memory, ImageRange image, Pin pin) {
    (void)address(memory, image, pin.rva, pin.size, RangeUse::image_code);
}
void exported(MemoryView memory, const ImageInfo& image, Entry entry) {
    const auto at = address(memory, image.range, entry.rva, 1, RangeUse::image_code);
    detail::MetadataView metadata{memory, image.range};
    const auto dir = image.directories[IMAGE_DIRECTORY_ENTRY_EXPORT];
    require(dir.rva && dir.size >= sizeof(IMAGE_EXPORT_DIRECTORY), "played export directory absent");
    const auto table = field<IMAGE_EXPORT_DIRECTORY>(metadata.view(), image.range, dir.rva);
    require(table.NumberOfFunctions && table.NumberOfFunctions <= 16384,
            "played export function table invalid");
    const auto target = inspect_export(metadata.view(), image, entry.ordinal);
    require(target.forwarder.empty() && target.address == at, "played export target changed");
}
void bindings(MemoryView memory, const NativeModules& modules) {
    for (const auto entry : core_exports) exported(memory, modules.afp, entry);
    for (const auto entry : utils_exports) exported(memory, modules.utils, entry);
    for (const auto binding : game_bindings(modules.game.range).played_calls_contract.game_imports) {
        const auto imported = inspect_import(memory, modules.game, binding.slot);
        require(imported.provider == "afp-core.dll" && imported.name.empty() &&
                imported.ordinal == binding.ordinal && imported.target ==
                address(memory, modules.afp.range, binding.target, 1, RangeUse::image_code),
                "played named import provider, ordinal or target changed");
    }
}
} // namespace

PlayedNativeCalls admit_played_calls(MemoryView memory, const NativeModules& modules) {
    return admit_played_calls(memory, modules,
        admit_cup_calls(memory, modules.afp, modules.utils));
}

PlayedNativeCalls admit_played_calls(MemoryView memory, const NativeModules& modules, const CupAfp& shared) {
    bindings(memory, modules);
    for (const auto pin : game_bindings(modules.game.range).played_calls_contract.game_code) code(memory, modules.game.range, pin);
    for (const auto pin : core_code) code(memory, modules.afp.range, pin);
    for (const auto pin : utils_code) code(memory, modules.utils.range, pin);
    const auto g = modules.game.range.base, c = modules.afp.range.base, u = modules.utils.range.base;
    PlayedNativeCalls result;
    result.chart.valid = shared.valid;
    result.chart.path = shared.path;
    result.chart.relative = shared.relative;
    result.chart.style = reinterpret_cast<decltype(result.chart.style)>(g + game_bindings(modules.game.range).played_calls_contract.chart_style_rva);
    result.package = shared.package;
    result.image_info = reinterpret_cast<decltype(result.image_info)>(u + 0xDC10);
    result.arguments = reinterpret_cast<decltype(result.arguments)>(u + 0xDCD0);
    result.animation_info = shared.animation_info;
    // 1EA70 consumes R8B, not a pointer: the existing int mode is ABI-compatible
    // for the renderer's mode=0. 22740 returns EAX=0; void intentionally ignores
    // it. 21800 has no reliable status result. 22BA0 consumes an int POINTER and
    // returns 0 on both branches (distinct from property's vararg value slot).
    result.bind = reinterpret_cast<decltype(result.bind)>(c + 0x1EA70);
    result.attach = shared.attach;
    result.control = shared.control;
    result.property = shared.property;
    result.path_relative = shared.path_relative;
    result.resolve = reinterpret_cast<decltype(result.resolve)>(c + 0x48AC0);
    result.definition = reinterpret_cast<decltype(result.definition)>(c + 0x521A0);
    result.select_stream = reinterpret_cast<decltype(result.select_stream)>(c + 0x41A0);
    result.restore_stream = reinterpret_cast<decltype(result.restore_stream)>(c + 0x4260);
    result.set_name = reinterpret_cast<decltype(result.set_name)>(c + 0x22740);
    result.clear_name = reinterpret_cast<decltype(result.clear_name)>(c + 0x3BB10);
    result.position = reinterpret_cast<decltype(result.position)>(c + 0x21800);
    result.timeline_position = reinterpret_cast<decltype(result.timeline_position)>(c + 0x22BA0);
    return result;
}

// Bounded call contract, not recursive admission of the native engine. Unlisted
// relative callees (including chart lookup/style internals, allocators, name
// storage and geometry), constants, stream globals, AVS/CRT imports and exception
// handlers are outside it. Played tuples come from the admitted capture sites.
// Capture/NOTES hook bodies, stock bitmap forwarding, package/owner layouts and
// native-thread lifetime/gating remain separate. Retain module leases and recheck
// ownership before use; returning typed addresses never executes those addresses.
} // namespace point_zero::native
