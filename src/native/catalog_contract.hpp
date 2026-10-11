#pragma once

#include "catalog_read.hpp"
#include "memory.hpp"

namespace point_zero::native {
struct CatalogContract {
    CatalogStorage storage;
    CatalogGetters getters;
    std::uintptr_t load{};
};

// LDJ2026081900 catalogue sub-contract ONLY, never generation admission or game
// readiness. Supply the actual selected image ranges (not filenames/hashes).
// An empty omnifix range selects stock. Checks required code extent/permissions,
// selected read-call/OmniFix replacement bytes, chain and storage ownership.
// Copies only consumed instructions, chain state and storage pointers. The
// parent supplies the independent six-byte loader hook prefix separately.
// Does not call game code, validate the imported provider, require load() to
// have returned zero, or capture loaded catalogue records. Those remain caller
// obligations; read_catalog validates the loaded header/layout/record contents.
// Observational checking cannot pin allocations or prevent changes after return.
// Throws std::runtime_error on a mismatch; never modifies observed memory.
CatalogContract check_catalog_contract(MemoryView, ImageRange game,
                                       ImageRange omnifix = {});
} // namespace point_zero::native
