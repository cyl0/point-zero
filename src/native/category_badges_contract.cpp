#include "build_bindings.hpp"
#include "category_badges_contract.hpp"

#include <stdexcept>

namespace point_zero::native {
namespace {
std::uintptr_t callable(MemoryView memory, ImageRange image, std::uint32_t rva) {
    if (rva >= image.size || !contains(image.base, image.size, image.base, image.size) ||
        !callable_range(memory, image.base + rva))
        throw std::runtime_error("category badge function unavailable");
    return image.base + rva;
}
} // namespace

CategoryBadgeCalls admit_category_badges_contract(MemoryView memory, const ImageInfo& game) {
    const auto& bindings = game_bindings(game.range).category_badges_contract;
    CategoryBadgeCalls result;
    result.music_owner = reinterpret_cast<decltype(result.music_owner)>(callable(memory, game.range, bindings.music_owner_rva));
    result.event_owner = reinterpret_cast<decltype(result.event_owner)>(callable(memory, game.range, bindings.event_owner_rva));
    result.chart_at = reinterpret_cast<decltype(result.chart_at)>(callable(memory, game.range, bindings.chart_at_rva));
    result.music_of = reinterpret_cast<decltype(result.music_of)>(callable(memory, game.range, bindings.music_of_rva));
    result.weekly = reinterpret_cast<decltype(result.weekly)>(callable(memory, game.range, bindings.weekly_rva));
    result.recommend = reinterpret_cast<decltype(result.recommend)>(callable(memory, game.range, bindings.recommend_rva));
    result.shop = reinterpret_cast<decltype(result.shop)>(callable(memory, game.range, bindings.shop_rva));
    result.kac = reinterpret_cast<decltype(result.kac)>(callable(memory, game.range, bindings.kac_rva));
    return result;
}
} // namespace point_zero::native
