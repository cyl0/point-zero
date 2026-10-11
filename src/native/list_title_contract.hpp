#pragma once

#include "list_title_hook.hpp"
#include "image.hpp"

namespace point_zero::native {
// Finite LDJ-010-2026081900 title renderer, scoped caller and stock width.
// Read-only admission returns the complete owned title body and inner site.
// No module loading, native calls, hook installation or global constant patch.
ListTitleSite admit_list_title_site(MemoryView, const ImageInfo& game);
} // namespace point_zero::native
