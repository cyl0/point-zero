#pragma once

#include "category_accessors.hpp"
#include "image.hpp"
#include <optional>

namespace point_zero::native {
// Finite LDJ-010-2026081900 / official Playlister 5.1.0 admission. Returns
// eight complete actual implementation bodies, plus the unpatched native
// style leaf. Known PL routes prove both incoming and saved-original chains;
// no arbitrary jump following or scanning. A getter still stock stays stock.
// Rival and style have no admitted PL replacement. The active getter supports
// SafetyHook's near layout only: its far stolen prefix contains a relative CALL
// which the official SafetyHook fallback explicitly refuses.
//
// Images are already inspected and retained. The caller owns serialization,
// module lifetime and subsequent hook operations; a range observation is not an
// atomic snapshot or a lifetime lease. Native manager/summary/category values,
// PL playlist storage, mutex/CRT state and external helper bodies are outside
// this finite slice. No game code executes during admission.
CategoryAccessorSites admit_category_accessors_contract(
    MemoryView, const ImageInfo& game, const std::optional<ImageInfo>& playlister);
} // namespace point_zero::native
