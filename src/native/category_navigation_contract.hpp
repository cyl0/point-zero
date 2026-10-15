#pragma once

#include "category_navigation.hpp"
#include "image.hpp"

namespace point_zero::native {
struct CategoryNavigationContract {
    CategoryNavigationSites sites;
    CategoryNavigationCalls calls;
};

// The caller retains callable targets and serializes hook installation.
CategoryNavigationContract admit_category_navigation_contract(MemoryView, const ImageInfo& game);
} // namespace point_zero::native
