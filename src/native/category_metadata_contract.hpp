#pragma once

#include "category_metadata.hpp"
#include "image.hpp"

namespace point_zero::native {
CategoryMetadataSite admit_category_metadata_contract(MemoryView, const ImageInfo& game);
}
