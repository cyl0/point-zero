#pragma once

#include "filter_hooks.hpp"
#include "image.hpp"

namespace point_zero::native {
struct FilterContract {
    FilterSites sites;
    int (__fastcall* mode)(){};
    NativeFilterRebuild rebuild{};
};

// Native summary must precede active-set restoration when rebuilding.
FilterContract admit_filter_contract(MemoryView, const ImageInfo& game);
} // namespace point_zero::native
