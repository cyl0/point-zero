#pragma once

#include "memory.hpp"
#include <cstdint>

namespace point_zero::native {
struct OriginalFilterState {
    std::uintptr_t category{}, manager{}, list{}, active{}, saved{};
    std::uintptr_t active_node{}, category_node{}, saved_node{}, object{}, control{};
    int id{}, style{}, lower{}, upper{};
    std::uint8_t enabled{};
    bool operator==(const OriginalFilterState&) const = default;
};

// Read only: identify an existing native level filter shared by active set1
// and this category/style's saved map. No get-or-create, shared retain/release,
// hash lookup, vtable change, editor mutation or process-wide pointer cache.
// Contract admission of the native map/menu layouts is a caller prerequisite.
// Captures the required owner fields once; clears out on failure.
bool read_original_filter_state(MemoryView, ImageRange game,
    std::uintptr_t category, int style, OriginalFilterState& out) noexcept;

enum class SavedFilterResult { none, ready, invalid };

// Same exact owner/predicate checks, but only traverse the saved maps.
// Active owner identity is copied; active nodes/predicate equality are not read.
// ready includes disabled/inverted predicates and always has active_node == 0.
// Disabled endpoints are preserved as dormant data (including saved 0/0);
// endpoints are compared as supplied, including inverted or nonstandard limits.
// none identifies a menu without set1 ownership (has_set == 0 or set != 1),
// or a valid saved list missing the category/key0. An unbound menu need not
// match the category ID; an owned set1 must. has_set uses native nonzero
// truth here; mutating rebuild admission separately requires canonical1. A
// mismatched owned-set1 menu ID or unreadable/malformed list is invalid.
// Both none and invalid clear out; layout admission remains a prerequisite.
SavedFilterResult read_saved_original_filter_state(MemoryView, ImageRange game,
    std::uintptr_t category, int style, OriginalFilterState& out) noexcept;
} // namespace point_zero::native
