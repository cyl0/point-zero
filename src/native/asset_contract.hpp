#pragma once

#include "assets.hpp"
#include "image.hpp"

namespace point_zero::native {
// Finite, read-only final-build resource-operation admission. Checks complete
// selected game/AFP-utils executable ranges, their package-operation import/export bindings,
// and writable manager storage including the native order array and mutex.
// Requires retained inspected modules. Performs no native calls, writes, hooks
// or filesystem access. AVS filesystem implementation/hook compatibility and
// actual loaded package contents/lifetimes remain separate contracts.
AssetNative admit_asset_calls(MemoryView, const ImageInfo& game,
                              const ImageInfo& utils, const ImageInfo& avs);
} // namespace point_zero::native
