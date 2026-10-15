#pragma once

#include "category_requests.hpp"
#include "image.hpp"

namespace point_zero::native {
CategoryRequestSites admit_category_request_sites(MemoryView, const ImageInfo& game);
}
