#include "build_bindings.hpp"
#include "category_restore_contract.hpp"

#include <stdexcept>

namespace point_zero::native {
namespace {
std::uintptr_t callable(MemoryView memory, ImageRange image, std::uint32_t rva) {
    if (rva >= image.size || !contains(image.base, image.size, image.base, image.size) ||
        !callable_range(memory, image.base + rva))
        throw std::runtime_error("category restore function unavailable");
    return image.base + rva;
}
} // namespace

CategoryRestoreCalls admit_category_restore_contract(MemoryView memory, const ImageInfo& game) {
    const auto& bindings = game_bindings(game.range).category_restore_contract;
    CategoryRestoreCalls result;
    result.current = reinterpret_cast<decltype(result.current)>(callable(memory, game.range, bindings.current_rva));
    result.restore = reinterpret_cast<decltype(result.restore)>(callable(memory, game.range, bindings.restore_rva));
    return result;
}
} // namespace point_zero::native
