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
    result.style_apply_caller = base + bindings.sites_style_apply_caller_rva;
    result.style_restore_caller = base + bindings.sites_style_restore_caller_rva;
    result.scene_vtable = base + bindings.scene_vtable_rva;
    result.idle_callback = base + bindings.idle_callback_rva;
    const auto queued = bindings.queued_restore;
    result.queued_restore = {base + queued.begin,
        read_code(memory, game.range, queued.begin, queued.end - queued.begin)};
    result.queue_caller = base + bindings.queue_caller_rva;
    result.initialize_vtable = base + bindings.initialize_vtable_rva;
    result.restore_vtable = base + bindings.restore_vtable_rva;
    result.restore_invoke = base + bindings.restore_invoke_rva;
    result.manager_slot = base + game_bindings(game.range).category_generations.category_manager_rva;
    result.restore_caller = base + bindings.restore_caller_rva;
    return result;
}
}
