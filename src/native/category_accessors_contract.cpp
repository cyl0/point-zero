#include "build_bindings.hpp"
#include "category_accessors_contract.hpp"

namespace point_zero::native {
CategoryAccessorSites admit_category_accessors_contract(MemoryView memory, const ImageInfo& game) {
    const auto& bindings = game_bindings(game.range).category_accessors_contract;
    CategoryAccessorSites result;
    for (std::size_t i = 0; i < result.entries.size(); ++i) {
        const auto pin = bindings.pins[i];
        result.entries[i] = {game.range.base + pin.begin,
            read_code(memory, game.range, pin.begin, pin.end - pin.begin)};
    }
    result.style = {game.range.base + bindings.category_style_rva,
        read_code(memory, game.range, bindings.category_style_rva, 21)};
    return result;
}
}
