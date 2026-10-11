#pragma once

#include "category_navigation.hpp"
#include "image.hpp"

namespace point_zero::native {
struct CategoryNavigationContract {
    CategoryNavigationSites sites;
    CategoryNavigationCalls calls;
};

// Finite, read-only LDJ-010-2026081900 admission; throws on refusal. Copies
// complete open/cycle/input/pressed/close/sound/exact/restore/set-difficulty
// bodies and the list vtable+8 binding once. Ordinary body edits are allowed.
// Caller retains the image and serializes installation and native lifetimes.
// This is neither an atomic snapshot nor recursive admission of external
// callees, mutable owners/input state, exception machinery or later code edits.
// Exact is AL category,song,p1,p2 (Windows x64); -1 difficulties stay sentinels.
// Restore copies chart original +10/+14 to working +8/+C. Set-difficulty is
// void(category,int difficulty,int player). Its metadata+9 locked branch avoids
// chart-row mutation; the unlocked branch does not. Separately admitted metadata
// routing and current locked private-leaf ownership are caller prerequisites.
// Exact's song search/current-chart/working-difficulty callees remain native
// forwarding dependencies, not new hooks or recursively admitted code.
// Admission executes no game code, installs no hooks and reads no live objects.
CategoryNavigationContract admit_category_navigation_contract(MemoryView, const ImageInfo& game);
} // namespace point_zero::native
