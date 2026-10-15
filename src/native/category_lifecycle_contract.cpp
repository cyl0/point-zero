#include "build_bindings.hpp"
#include "category_lifecycle_contract.hpp"

namespace point_zero::native {
CategoryLifeSites admit_category_lifecycle_contract(MemoryView memory, const ImageInfo& game) {
    const auto& bindings = game_bindings(game.range).category_lifecycle_contract;
    CategoryLifeSites result;
    for (std::size_t i = 0; i < result.entries.size(); ++i) {
        const auto pin = bindings.pins[i];
        result.entries[i] = {game.range.base + pin.begin,
            read_code(memory, game.range, pin.begin, pin.end - pin.begin)};
    }
    const auto base = game.range.base;
    result.initialize_caller = base + bindings.sites_initialize_caller_rva;
    result.scene_entry_caller = base + bindings.sites_scene_entry_caller_rva;
    result.scene_tick_caller = base + bindings.sites_scene_tick_caller_rva;
    result.scene_refresh_caller = base + bindings.sites_scene_refresh_caller_rva;
    result.preview_caller = base + bindings.sites_preview_caller_rva;
    result.restore_caller = base + bindings.sites_restore_caller_rva;
    result.style_apply_caller = base + bindings.sites_style_apply_caller_rva;
    result.style_restore_caller = base + bindings.sites_style_restore_caller_rva;
    result.scene_vtable = base + bindings.scene_vtable_rva;
    result.idle_callback = base + bindings.idle_callback_rva;
    return result;
}
}
