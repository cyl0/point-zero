#pragma once

#include "category_metadata.hpp"

#include <array>

namespace point_zero::native {
// Native metadata only: the existing row/hover consumers retain their drawing,
// placement and animation. Strings have static storage; CategoryMetadata also
// copies them before exposing its process-resident definitions.
std::array<CategoryMetadataEntry, 144> folder_definitions() noexcept;
} // namespace point_zero::native
