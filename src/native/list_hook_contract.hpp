#pragma once

#include "list_hook.hpp"
#include "image.hpp"

namespace point_zero::native {
// Finite LDJ-010-2026081900 shared named-image entry and four list callers.
// Read-only admission; returns an owned copy of the verified displaced emitter prefix.
// No module loading, native calls, hook installation or title-hook admission.
ListHookSite admit_list_hook_site(MemoryView, const ImageInfo& game);
} // namespace point_zero::native
