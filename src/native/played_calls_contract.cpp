#include "build_bindings.hpp"
#include "played_calls_contract.hpp"

#include <stdexcept>

namespace point_zero::native {
namespace {
std::uintptr_t code(MemoryView memory, ImageRange image, std::uint32_t rva) {
    const auto address = image.base + rva;
    if (!contains(image.base, image.size, address, 1) || !callable_range(memory, address))
        throw std::runtime_error("played call unavailable");
    return address;
}
std::uintptr_t target(MemoryView memory, ImageRange image, std::uint32_t rva) {
    std::uintptr_t result{};
    if (!contains(image.base, image.size, image.base + rva, sizeof(result)) ||
        !read_value(memory, image.base + rva, result) || !callable_range(memory, result))
        throw std::runtime_error("played dispatch unavailable");
    return result;
}
}
PlayedNativeCalls admit_played_calls(MemoryView memory, const NativeModules& modules) {
    return admit_played_calls(memory, modules, admit_cup_calls(memory, modules.afp, modules.utils));
}

PlayedNativeCalls admit_played_calls(MemoryView memory, const NativeModules& modules, const CupAfp& shared) {
    const auto game = modules.game.range, core = modules.afp.range;
    const auto& bindings = game_bindings(game).played_calls_contract;
    PlayedNativeCalls result;
    result.chart.valid = shared.valid;
    result.chart.path = shared.path;
    result.chart.relative = reinterpret_cast<decltype(result.chart.relative)>(target(memory, game, bindings.relative_import_rva));
    result.chart.style = reinterpret_cast<decltype(result.chart.style)>(code(memory, game, bindings.chart_style_rva));
    result.package = shared.package;
    result.image_info = reinterpret_cast<decltype(result.image_info)>(resolve_export(memory, modules.utils, 71));
    result.arguments = reinterpret_cast<decltype(result.arguments)>(resolve_export(memory, modules.utils, 77));
    result.animation_info = shared.animation_info;
    result.bind = reinterpret_cast<decltype(result.bind)>(resolve_export(memory, modules.afp, 137));
    result.attach = shared.attach;
    result.control = shared.control;
    result.property = reinterpret_cast<decltype(result.property)>(target(memory, game, bindings.property_import_rva));
    result.path_relative = shared.path_relative;
    result.resolve = reinterpret_cast<decltype(result.resolve)>(code(memory, core, 0x48AC0));
    result.definition = reinterpret_cast<decltype(result.definition)>(code(memory, core, 0x521A0));
    result.select_stream = reinterpret_cast<decltype(result.select_stream)>(code(memory, core, 0x41A0));
    result.restore_stream = reinterpret_cast<decltype(result.restore_stream)>(code(memory, core, 0x4260));
    result.set_name = reinterpret_cast<decltype(result.set_name)>(code(memory, core, 0x22740));
    result.clear_name = reinterpret_cast<decltype(result.clear_name)>(code(memory, core, 0x3BB10));
    result.position = reinterpret_cast<decltype(result.position)>(code(memory, core, 0x21800));
    result.timeline_position = reinterpret_cast<decltype(result.timeline_position)>(target(memory, core, 0xDD390 + 8 * 30));
    return result;
}
} // namespace point_zero::native
