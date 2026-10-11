#pragma once

#include "prepared.hpp"
#include "native/catalog_read.hpp"
#include "native/feature_state.hpp"

namespace point_zero::core {
// Internal initialization boundary, not a DLL ABI or an extensible plugin API.
// The concrete game driver owns admitted modules, startup hook, private assets
// and the selected native adapters. Tests provide an explicitly isolated driver.
struct NativeSetup {
    std::filesystem::path runtime, generation;
    std::string generation_id;
    // These callbacks belong to the retained core owner. It serializes access
    // to the actual SDK host and makes them inert before shutdown returns.
    PZ_NativeHost host;
    std::string release_version;
};
class NativeDriver {
public:
    using Observer = void (*)(void*, int) noexcept;
    virtual ~NativeDriver() = default;
    virtual void install(Observer, void*) = 0;
    virtual native::CapturedCatalog capture() = 0;
    // All required adapters/resources must be activated together while the
    // supplied shared state is still preparing. No publication or ACK here.
    virtual void activate(const native::Catalog&) = 0;
    // Only before publication, on the admitted native thread if native work
    // began. False means clean ownership/withdrawal could not be established.
    virtual bool rollback() noexcept = 0;
    // Drain startup observation, then any subsequently installed adapter
    // callbacks; all native forwarding/backing remains alive.
    virtual void retire_observer() noexcept = 0;
};

// Defined by native/game_driver.cpp. Its native adapter factory is deliberately
// required at final link: there is no production admission-success substitute.
std::unique_ptr<NativeDriver> make_native_driver(const NativeSetup&, native::FeatureState&);
} // namespace point_zero::core
