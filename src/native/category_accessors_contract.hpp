#pragma once

#include "category_accessors.hpp"
#include "image.hpp"

namespace point_zero::native {
CategoryAccessorSites admit_category_accessors_contract(MemoryView, const ImageInfo& game);
}
