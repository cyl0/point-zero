#pragma once

#include "image.hpp"
#include "sort_hook.hpp"

namespace point_zero::native {
struct SortContract {
    SortNativeCalls calls;
    SortHookSite hook;
};

// Read-only admission of the finite LDJ-010-2026081900 visible-sort slice.
// Requires a structurally inspected, retained game image. Throws on changed
// selected instructions, bounds, allocation or permissions; calls no native function and
// installs no hook. Returns typed mode/comparator addresses and owned qsort
// bytes. Incoming comparator/category/visible/player gates belong to the sorter.
SortContract admit_sort_contract(MemoryView, const ImageInfo& game);
} // namespace point_zero::native
