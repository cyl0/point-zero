#pragma once

#include "category_requests.hpp"
#include "image.hpp"

namespace point_zero::native {
// save takes the native map at settings()+48h; clear(settings,1) clears it.
// persistent returns AL. save, clear and group provide no success status.
CategoryRequestCalls admit_category_request_calls(MemoryView, const ImageInfo& game);
} // namespace point_zero::native
