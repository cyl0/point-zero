#pragma once

#include "image.hpp"
#include <optional>

namespace point_zero::native {
// Required metadata consumers, not a new renderer. Shared named emission,
// bitmap binding and resource calls are admitted by their respective owners.
// No code patch, native call, or scene/resource access.
void admit_folder_consumers(MemoryView, const ImageInfo& game,
                            const std::optional<ImageInfo>& playlister = std::nullopt);
// After native graphics/resources are initialized. Admit the registered core
// bitmap resolver and its active native function-table route to selected utils.
// Separate from both the surface-owned bitmap patch and the utils call contract.
void admit_folder_resolver(MemoryView, const ImageInfo& core, const ImageInfo& utils);
} // namespace point_zero::native
