#include "build_bindings.hpp"
#include "list_contract.hpp"

#include <stdexcept>

namespace point_zero::native {
namespace {
std::uintptr_t code(MemoryView memory, ImageRange image, std::uint32_t rva) {
    const auto address = image.base + rva;
    if (!contains(image.base, image.size, address, 1) || !callable_range(memory, address))
        throw std::runtime_error("list call unavailable");
    return address;
}
}
ListNativeCalls admit_list_calls(MemoryView memory, const NativeModules& modules) {
    const auto game = modules.game.range;
    const auto& bindings = game_bindings(game).list_contract;
    ListNativeCalls result;
    result.chart.player_count = reinterpret_cast<decltype(result.chart.player_count)>(code(memory, game, bindings.chart_player_count_rva));
    result.chart.style = reinterpret_cast<decltype(result.chart.style)>(code(memory, game, bindings.chart_style_rva));
    result.chart.active_player = reinterpret_cast<decltype(result.chart.active_player)>(code(memory, game, bindings.chart_active_player_rva));
    result.valid = reinterpret_cast<decltype(result.valid)>(resolve_export(memory, modules.afp, 22));
    result.package = reinterpret_cast<decltype(result.package)>(resolve_export(memory, modules.utils, 56));
    result.image_info = reinterpret_cast<decltype(result.image_info)>(resolve_export(memory, modules.utils, 71));
    result.arguments = reinterpret_cast<decltype(result.arguments)>(resolve_export(memory, modules.utils, 77));
    result.creation_level = reinterpret_cast<decltype(result.creation_level)>(resolve_export(memory, modules.afp, 21));
    result.set_creation_level = reinterpret_cast<decltype(result.set_creation_level)>(resolve_export(memory, modules.afp, 20));
    result.emit = reinterpret_cast<decltype(result.emit)>(code(memory, game, bindings.emit_rva));
    return result;
}
} // namespace point_zero::native
