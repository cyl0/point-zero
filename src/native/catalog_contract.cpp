#include "build_bindings.hpp"
#include "catalog_contract.hpp"
#include <stdexcept>

namespace point_zero::native {
CatalogContract check_catalog_contract(MemoryView memory, ImageRange game) {
    const auto& bindings = game_bindings(game).catalog_contract;
    for (const auto rva : {bindings.catalog_getters_rva, bindings.catalog_load_rva})
        if (!contains(game.base, game.size, game.base, game.size) || rva >= game.size ||
            !callable_range(memory, game.base + rva))
            throw std::runtime_error("catalogue function unavailable");
    return {{game.base + bindings.catalog_getters_rva}, game.base + bindings.catalog_load_rva};
}
}
