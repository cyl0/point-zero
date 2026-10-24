#include "build_bindings.hpp"
#include "category_request_sites_contract.hpp"

namespace point_zero::native {
CategoryRequestSites admit_category_request_sites(MemoryView memory, const ImageInfo& game) {
    const auto& bindings = game_bindings(game.range).category_request_sites_contract;
    CategoryRequestSites result;
    result.game = game.range;
    result.manager_getter = game.range.base + bindings.pins[3].begin;
    for (std::size_t i = 0; i < result.entries.size(); ++i) {
        const auto pin = bindings.pins[i];
        result.entries[i] = {game.range.base + pin.begin,
            read_code(memory, game.range, pin.begin, pin.end - pin.begin),
            game.range.base + bindings.captures[i], game.range.base + bindings.cancels[i],
            game.range.base + bindings.callers[i]};
    }
    return result;
}
}
