#pragma once

#include "catalog.hpp"
#include "feature_state.hpp"
#include "image.hpp"
#include "point_zero/core_api.h"

#include <memory>

namespace point_zero::native {
struct NativeModules {
    ImageInfo game, afp, utils;
};
struct AdapterContext {
    const NativeModules& modules;
    MemoryView memory;
    const Catalog& catalog;
    FeatureState& state;
    PZ_NativeHost host; // Retained core wrapper, never the raw SDK callbacks.
};
struct LoadedAsset { std::string_view family; std::uintptr_t slot; };

class NativeAdapters {
public:
    virtual ~NativeAdapters() = default;
    virtual void prepare() = 0;
    virtual std::span<const std::string_view> asset_families() const noexcept = 0;
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
