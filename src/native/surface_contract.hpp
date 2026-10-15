#pragma once

#include "image.hpp"
#include "surface_hooks.hpp"

namespace point_zero::native {
SurfaceHookSites admit_surface_sites(MemoryView, const ImageInfo& game,
                                     const ImageInfo& afp);
} // namespace point_zero::native
