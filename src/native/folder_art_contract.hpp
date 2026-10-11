#pragma once

#include "folder_art.hpp"
#include "image.hpp"

namespace point_zero::native {
// Read-only, finite LDJ-010-2026081900 utils admission. The caller retains the
// inspected image throughout package loading. No native execution,
// resource-state admission, game/core patch ownership or recursive AVS gate.
// The core-aware consumer contract separately proves the selected mutable
// resolver binding reaches the admitted utils185A0 -> DBB0 -> DB50 path.
FolderArtCalls admit_folder_art_calls(MemoryView, const ImageInfo& utils);
} // namespace point_zero::native
