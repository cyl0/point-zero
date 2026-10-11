#pragma once

#include "image.hpp"

namespace point_zero::native {
// LDJ2026081900's AVS ordinal82 import/export boundary. The launcher owns file
// interception; no Spice/IFS offsets, body hashes or private hook-chain inspection.
// Holds no module/allocation lifetime, installs nothing and calls no game code.
// This is not catalogue, resource, or complete generation admission.
void check_avs_read_contract(MemoryView, const ImageInfo& game, const ImageInfo& avs);
} // namespace point_zero::native
