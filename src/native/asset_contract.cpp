#include "build_bindings.hpp"
#include "asset_contract.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace point_zero::native {
namespace {
constexpr std::size_t order_offset = AssetOwner::slot_count * AssetOwner::slot_stride;
struct Tail {
    std::array<std::int32_t, AssetOwner::slot_count> order;
    std::uint32_t mutex;
};
static_assert(sizeof(Tail) == 0x804);
void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
std::uintptr_t manager(MemoryView memory, ImageRange game) {
    constexpr auto size = order_offset + sizeof(Tail);
    const auto begin = game.base + game_bindings(game).asset_contract.manager_rva;
    require(contains(game.base, game.size, begin, size), "asset manager outside selected game");
    for (auto at = begin; at < begin + size;) {
        MemoryRegion region{};
        require(memory.query && memory.query(memory.context, at, region) &&
                contains(region.begin, region.size, at, 1) && region.committed &&
                region.readable && region.writable && !region.guarded,
                "asset manager storage unavailable");
        at = (std::min)(begin + size, region.begin + region.size);
    }
    Tail tail{};
    require(read_value(memory, begin + order_offset, tail) && tail.mutex,
            "asset manager is not initialized");
    require(std::all_of(tail.order.begin(), tail.order.end(), [](auto value) {
        return value < static_cast<std::int32_t>(AssetOwner::slot_count);
    }), "asset manager order index is outside the slot array");
    return begin;
}
}
AssetNative admit_asset_calls(MemoryView memory, const ImageInfo& game) {
    const auto& bindings = game_bindings(game.range).asset_contract;
    AssetNative result{manager(memory, game.range), game.range.base + bindings.load_rva,
        game.range.base + bindings.release_rva};
    require(contains(game.range.base, game.range.size, result.load, 1) &&
            contains(game.range.base, game.range.size, result.release, 1) &&
            callable_range(memory, result.load) && callable_range(memory, result.release),
            "asset operation unavailable");
    return result;
}
} // namespace point_zero::native
