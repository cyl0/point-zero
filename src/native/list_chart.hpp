#pragma once

#include "catalog.hpp"
#include "memory.hpp"

namespace point_zero::native {
struct ListChartCalls {
    int (__fastcall* player_count)(){};
    int (__fastcall* style)(){};
    int (__fastcall* active_player)(){};
};
struct ListChart {
    bool compact{};
    const CatalogRow* chart{}; // Retained catalogue row; native owner is ephemeral.
};
// LDJ2026081900 admitted draw context. Full return address distinguishes solo,
// paired equal and paired different-difficulty labels. incoming is the
// original native pointer, compared with the safely read live table slot;
// badge identity never dereferences it.
// Native calls must already be admitted; failures/SEH clear out
// and return false without changing shared state. No allocation or native writes.
bool read_list_chart(MemoryView, ImageRange game, const Catalog&, ListChartCalls,
                     std::uintptr_t owner, std::uintptr_t return_address,
                     const char* incoming, ListChart& out) noexcept;
} // namespace point_zero::native
