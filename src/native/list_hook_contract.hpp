#pragma once

#include "list_hook.hpp"
#include "image.hpp"

namespace point_zero::native {
// Read-only admission of the four selected game CALL sites.
ListHookSite admit_list_hook_site(MemoryView, const ImageInfo& game);
} // namespace point_zero::native
