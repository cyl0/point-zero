#include "build_bindings.hpp"
#include "category_navigation_contract.hpp"

#include <stdexcept>
#include <vector>

namespace point_zero::native {
namespace {
void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
std::uintptr_t address(ImageRange image, std::uint32_t rva, std::size_t size = 1) {
    require(image.base >= 0x10000 && contains(image.base, image.size, image.base, image.size) &&
            rva <= image.size && size <= image.size - rva,
            "category navigation extent outside selected image");
    return image.base + rva;
}
std::uintptr_t callable(MemoryView memory, ImageRange image, std::uint32_t rva) {
    const auto at = address(image, rva);
    require(callable_range(memory, at), "category navigation function unavailable");
    return at;
}
} // namespace

CategoryNavigationContract admit_category_navigation_contract(MemoryView memory, const ImageInfo& game) {
    const auto& bindings = game_bindings(game.range).category_navigation_contract;
    CategoryNavigationContract result;
    result.sites.open = {address(game.range, bindings.open_rva, 32),
        read_code(memory, game.range, bindings.open_rva, 32)};
    const auto slot = address(game.range, bindings.cycle_slot, sizeof(std::uintptr_t));
    std::uintptr_t target{};
    require(read_value(memory, slot, target) && callable_range(memory, target, 32),
            "category navigation cycle target unavailable");
    result.sites.cycle = {target, std::vector<std::byte>(32)};
    require(read_bytes(memory, target, result.sites.cycle.body), "category navigation cycle read failed");
    result.sites.exact = {address(game.range, bindings.exact_rva, 32),
        read_code(memory, game.range, bindings.exact_rva, 32)};
    result.sites.cycle_caller = address(game.range, bindings.sites_cycle_caller_rva);
    result.sites.back = {address(game.range, bindings.sites_back_rva, 5),
        read_code(memory, game.range, bindings.sites_back_rva, 5)};
    result.sites.back_continuation = callable(memory, game.range, bindings.sites_back_continuation_rva);
    result.calls.close = reinterpret_cast<decltype(result.calls.close)>(callable(memory, game.range, bindings.close_rva));
    result.calls.sound = reinterpret_cast<decltype(result.calls.sound)>(callable(memory, game.range, bindings.sound_rva));
    result.calls.restore_chart = reinterpret_cast<decltype(result.calls.restore_chart)>(callable(memory, game.range, bindings.restore_chart_rva));
    result.calls.set_difficulty = reinterpret_cast<decltype(result.calls.set_difficulty)>(callable(memory, game.range, bindings.set_difficulty_rva));
    return result;
}
} // namespace point_zero::native
