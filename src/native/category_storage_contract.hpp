#pragma once

#include "category_storage.hpp"
#include "image.hpp"

namespace point_zero::native {
// The caller owns category lifetimes and keeps auxiliary storage empty.
CategoryNativeCalls admit_category_storage_contract(MemoryView, const ImageInfo&);
} // namespace point_zero::native
