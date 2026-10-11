#pragma once

#include "filter_hooks.hpp"
#include "image.hpp"

namespace point_zero::native {
struct FilterContract {
    FilterSites sites;
    int (__fastcall* mode)(){};
    NativeFilterRebuild rebuild{};
};

// Finite, read-only LDJ-010-2026081900 admission. Requires a structurally
// inspected, retained game image AND GameDriver's catalogue admission (which
// owns 951E80 and its supported Omni dependencies). Calls no native function.
// Throws on changed selected bodies/bindings, range, allocation or permissions.
// Map ownership, captured category/style, live values and hook lifetime remain
// the reader/hook's responsibility. Returned snapshots own their bytes.
// Rebuild is returned only after both passes. Invoking it alone does not prove
// coherent filter/toggle composition: native summary precedes active-set restore.
FilterContract admit_filter_contract(MemoryView, const ImageInfo& game);
} // namespace point_zero::native
