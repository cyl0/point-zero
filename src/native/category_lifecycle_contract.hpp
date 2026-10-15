#pragma once

#include "category_lifecycle_hooks.hpp"
#include "image.hpp"

namespace point_zero::native {
CategoryLifeSites admit_category_lifecycle_contract(MemoryView, const ImageInfo& game);
}
