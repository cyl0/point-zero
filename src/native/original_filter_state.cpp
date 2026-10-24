#include "build_bindings.hpp"
#include "original_filter_state.hpp"

#include <windows.h>

namespace point_zero::native {
namespace {
constexpr std::size_t max_entries = 4096;
bool pointer(std::uintptr_t value) noexcept { return value >= 0x10000 && (value & 7) == 0; }



// Native owns the map. Find the requested key without auditing unrelated
// entries, backwards links, reference counts, or a second copy of the list.
bool node_for(MemoryView memory, std::uintptr_t map, int key, std::uintptr_t& found) noexcept {
    found = 0;
    std::uintptr_t head{}, node{};
    if (!pointer(map) || !read_field(memory, map, 8, head) || !pointer(head) || !read_field(memory, head, 0, node)) return false;
    for (std::size_t i = 0; node != head && i < max_entries; ++i) {
        int actual{};
        if (!pointer(node) || !read_field(memory, node, 16, actual)) return false;
        if (actual == key) { found = node; return true; }
        if (!read_field(memory, node, 0, node)) return false;
    }
    return node == head;
}

SavedFilterResult collect(MemoryView memory, ImageRange game, std::uintptr_t category,
                          int style, bool saved_only, OriginalFilterState& out) noexcept {
    if (style < 0 || style > 1 || !pointer(category) || !pointer(game.base) ||
        !contains(game.base, game.size, game.base, game.size) || game.size < game_bindings(game).original_filter_state.saved_filter_rva + 8)
        return SavedFilterResult::invalid;
    OriginalFilterState next;
    next.category = category; next.style = style;
    std::uintptr_t vt{}, active_object{}, object_vt{};
    int menu_id{}, menu_set{};
    std::uint8_t has_set{};
    if (!read_field(memory, category, 0, next.id) ||
        !(next.id == 119 || (next.id >= 219 && next.id <= 223)) ||
        !read_field(memory, game.base, game_bindings(game).original_filter_state.category_manager_rva, next.manager) || !pointer(next.manager) ||
        !contains(next.manager, 16 + 2 * 226 * 208, category, 208) ||
        category != next.manager + 16 + 208 * (226ULL * style + next.id) ||
        !read_field(memory, next.manager, 0x16F50, next.list) || !pointer(next.list) ||
        !read_field(memory, next.list, 0, vt) || vt != game.base + game_bindings(game).original_filter_state.list_vtable_rva ||
        !read_field(memory, category, 160, menu_id) || (!saved_only && menu_id != next.id) ||
        !read_field(memory, category, 164, menu_set) || (!saved_only && menu_set != 1) ||
        !read_field(memory, category, 168, has_set) || (!saved_only && !has_set))
        return SavedFilterResult::invalid;
    if (saved_only) {
        // A fresh search category119 can retain an unbound menu ID.
        // Match saved state only after native has bound the owned set1 menu.
        if (menu_set != 1 || has_set == 0)
            return SavedFilterResult::none;
        if (menu_id != next.id) return SavedFilterResult::invalid;
    }
    if (!read_field(memory, game.base, game_bindings(game).original_filter_state.active_filter_rva, next.active) || !pointer(next.active) ||
        !read_field(memory, game.base, game_bindings(game).original_filter_state.saved_filter_rva, next.saved) || !pointer(next.saved) ||
        next.active > UINTPTR_MAX - 200 || next.saved > UINTPTR_MAX - 136 ||
        (!saved_only && (!node_for(memory, next.active + 8 + 64, 0, next.active_node) || !next.active_node)) ||
        !node_for(memory, next.saved + 8 + 64ULL * style, next.id, next.category_node))
        return SavedFilterResult::invalid;
    if (!next.category_node)
        return saved_only ? SavedFilterResult::none : SavedFilterResult::invalid;
    if (next.category_node > UINTPTR_MAX - 24 ||
        !node_for(memory, next.category_node + 24, 0, next.saved_node)) return SavedFilterResult::invalid;
    if (!next.saved_node)
        return saved_only ? SavedFilterResult::none : SavedFilterResult::invalid;
    if ((!saved_only && (!read_field(memory, next.active_node, 24, active_object))) ||
        !read_field(memory, next.saved_node, 24, next.object) || !read_field(memory, next.saved_node, 32, next.control) ||
        (!saved_only && active_object != next.object) ||
        !pointer(next.object) || !read_field(memory, next.object, 0, object_vt) ||
        object_vt != game.base + game_bindings(game).original_filter_state.filter_vtable_rva ||
        !read_field(memory, next.object, 8, next.enabled) ||
        !read_field(memory, next.object, 16, next.lower) || !read_field(memory, next.object, 20, next.upper))
        return SavedFilterResult::invalid;
    // Native owns the enabled byte and editor limits. Bounds are just integer
    // comparisons, so neither disabled 0/0 nor a nonstandard range is invalid.
    out = next;
    return SavedFilterResult::ready;
}
} // namespace

bool read_original_filter_state(MemoryView memory, ImageRange game,
    std::uintptr_t category, int style, OriginalFilterState& out) noexcept {
    out = {};
    __try { return collect(memory, game, category, style, false, out) == SavedFilterResult::ready; }
    __except (EXCEPTION_EXECUTE_HANDLER) { out = {}; return false; }
}

SavedFilterResult read_saved_original_filter_state(MemoryView memory, ImageRange game,
    std::uintptr_t category, int style, OriginalFilterState& out) noexcept {
    out = {};
    __try { return collect(memory, game, category, style, true, out); }
    __except (EXCEPTION_EXECUTE_HANDLER) { out = {}; return SavedFilterResult::invalid; }
}
} // namespace point_zero::native
