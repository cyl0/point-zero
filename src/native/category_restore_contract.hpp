#pragma once

#include "image.hpp"

namespace point_zero::native {
struct CategoryRestoreCalls {
    std::uintptr_t (__fastcall* current)(std::uintptr_t navigation){};
    std::uintptr_t (__fastcall* restore)(std::uintptr_t selection, int category, int song,
                                        int p1, int p2, std::uint8_t force_open, int extra){};
};

// Finite, read-only LDJ-010-2026081900 admission; throws on invalid ranges.
// Ordinary function bodies are read once without digest comparisons.
// Returns both pointers only after complete success. restore's native RAX is
// unspecified, NOT a success boolean; its tail event-5 dispatch must complete.
//
// The caller retains the image, serializes native use and owns destination
// policy: valid nonempty current group/category, settled native bank/style,
// saved scalar chart identity and working preferences, map/category lifetimes,
// and publication of the prepared destination. Admission does not supply any
// of those guarantees or preserve the selected working tuple/non-chart selection.
// Summary/rebuild can change working rows: the parent must compose post-rebuild
// working restoration BEFORE native sort/search. The graph can retain the
// category after return. Compose with filter, sort, lifecycle and navigation
// admission; their dependencies are not re-admitted here. Other engine/CRT/EH
// callees and mutable state are outside this finite scope.
CategoryRestoreCalls admit_category_restore_contract(MemoryView, const ImageInfo& game);
} // namespace point_zero::native
