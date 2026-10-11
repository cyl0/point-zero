#pragma once

#include "catalog.hpp"
#include "memory.hpp"
#include <cstddef>
#include <cstdint>
#include <span>

namespace point_zero::native {
// Storage and function addresses must already have passed the selected
// datecode/foreign-hook contracts. This reader validates data, not code ownership.
struct CatalogStorage {
    std::uintptr_t address{};
    std::size_t capacity{}; // Reviewed stock 4 MiB or OmniFix 6 MiB allocation.
};
struct CatalogAccess {
    CatalogStorage storage;
    // Caller supplies the guarded copy provider; parsing bounds each snapshot
    // against admitted storage and does not query every native field.
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
CapturedCatalog read_current_catalog(CatalogStorage, CatalogGetters);
} // namespace point_zero::native
