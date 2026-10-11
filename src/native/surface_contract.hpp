#pragma once

#include "image.hpp"
#include "surface_hooks.hpp"

namespace point_zero::native {
// Finite LDJ-2026081900 cup-update, AFP named-bitmap entry and selected game
// import binding contracts. The native game must call the intercepted entry.
// Reads only, returns copied displaced prefixes; no module loading, hook or native call.
SurfaceHookSites admit_surface_sites(MemoryView, const ImageInfo& game,
                                     const ImageInfo& afp);
} // namespace point_zero::native
