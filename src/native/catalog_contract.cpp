#include "build_bindings.hpp"
#include "catalog_contract.hpp"
#include <stdexcept>

namespace point_zero::native {
CatalogContract check_catalog_contract(MemoryView memory, ImageRange game) {
    const auto& bindings = game_bindings(game).catalog_contract;
    for (const auto& pin : bindings.pins) {
        if (!contains(game.base, game.size, game.base, game.size) || pin.rva > game.size ||
            pin.size > game.size - pin.rva ||
            !readable_range(memory, game.base + pin.rva, pin.size, RangeUse::image_code, game))
            throw std::runtime_error("catalogue game code extent or protection changed");
    }
    return {{game.base + bindings.catalog_getters_rva}, game.base + bindings.catalog_load_rva};
}
}
