#pragma once
#include "catalog_read.hpp"

namespace point_zero::native {
struct CatalogContract {
    CatalogGetters getters;
    std::uintptr_t load{};
};
// Bind the selected game build; the loaded catalogue is validated after load returns.
CatalogContract check_catalog_contract(MemoryView, ImageRange game);
}
