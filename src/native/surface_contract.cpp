#include "build_bindings.hpp"
#include "surface_contract.hpp"

#include <stdexcept>

namespace point_zero::native {
namespace {
SurfaceHookSite entry(MemoryView memory, std::uintptr_t address) {
    SurfaceHookSite result{address, std::vector<std::byte>(32)};
    if (!callable_range(memory, address, result.body.size()) || !read_bytes(memory, address, result.body))
        throw std::runtime_error("surface entry unavailable");
    return result;
}
}
SurfaceHookSites admit_surface_sites(MemoryView memory, const ImageInfo& game, const ImageInfo&) {
    const auto& bindings = game_bindings(game.range).surface_contract;
    std::uintptr_t bitmap{};
    if (!contains(game.range.base, game.range.size, game.range.base + bindings.cup_rva, 32) ||
        !contains(game.range.base, game.range.size, game.range.base + bindings.bitmap_import, sizeof(bitmap)) ||
        !read_value(memory, game.range.base + bindings.bitmap_import, bitmap))
        throw std::runtime_error("surface interface unavailable");
    return {entry(memory, game.range.base + bindings.cup_rva), entry(memory, bitmap)};
}
} // namespace point_zero::native
