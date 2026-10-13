#pragma once

#include "catalog.hpp"
#include "memory.hpp"
#include <cstddef>
#include <cstdint>
#include <span>

namespace point_zero::native {
struct CatalogAccess {
    // Guarded copy provider; the snapshot is bounded independently of its provider.
    MemoryView memory;
    void* header_context{};
    bool (*header)(void*, std::uintptr_t&) noexcept{};
};
struct CapturedCatalog {
    // Header counts are parser-local; consumers need registered charts only.
    std::vector<RegisteredChart> charts;
};

// Called once at the admitted catalogue-loaded boundary. Throws on an invalid
// or changing catalogue, returning no partial result. No eligibility/score work.
CapturedCatalog read_catalog(const CatalogAccess&);

struct CatalogGetters {
    std::uintptr_t header{};
};
// Current-process guarded reads/calls only. Does not open another process,
// resolve/scan addresses, install hooks or write native memory.
CapturedCatalog read_current_catalog(CatalogGetters);
} // namespace point_zero::native
