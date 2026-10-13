#pragma once

#include "catalog.hpp"
#include "feature_state.hpp"
#include "image.hpp"
#include "point_zero/core_api.h"

#include <memory>
#include <optional>

namespace point_zero::native {
struct NativeModules {
    ImageInfo game, avs, afp, utils;
    std::optional<ImageInfo> playlister, fervidex;
};
struct AdapterContext {
    const NativeModules& modules;
    MemoryView memory;
    const Catalog& catalog;
    FeatureState& state;
    PZ_NativeHost host; // Retained core wrapper, never the raw SDK callbacks.
};
struct LoadedAsset { std::string_view family; std::uintptr_t slot; };

// One selected native implementation, not a runtime registry/plugin broker.
// Construction is inert. prepare validates ALL selected surface/browsing/mode
// contracts and creates their hooks disabled. No catalogue-only success path.
class NativeAdapters {
public:
    virtual ~NativeAdapters() = default;
    virtual void prepare() = 0;
    virtual std::span<const std::string_view> asset_families() const noexcept = 0;
    // Validate private resources/owner bindings, then enable every required
    // hook with the shared gate still closed. Throw on any incomplete result.
    // No native node/stream may retain a package before final publication.
    // The span is borrowed for this call; copy any retained slot values.
    virtual void activate(std::span<const LoadedAsset>) = 0;
    // Undo only proven prepublication effects; never free executing trampolines
    // or uncertain owners. This object remains alive after any exposed callback.
    virtual bool rollback() noexcept = 0;
    // Called only after startup observation has drained. Retire all exposed
    // surface/browsing callbacks without freeing native forwarding/backing or
    // touching native consumer resources from the shutdown thread.
    virtual void retire_callbacks() noexcept = 0;
};

// Required final-link implementation. Deliberately no default success adapter.
std::unique_ptr<NativeAdapters> make_native_adapters(const AdapterContext&);
} // namespace point_zero::native
