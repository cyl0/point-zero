#include "build_bindings.hpp"
#include "category_vector_contract.hpp"

#include <stdexcept>

namespace point_zero::native {
namespace {
std::uintptr_t callable(MemoryView memory, ImageRange image, std::uint32_t rva) {
    if (!contains(image.base, image.size, image.base, image.size) || rva >= image.size ||
        !callable_range(memory, image.base + rva))
        throw std::runtime_error("category vector function unavailable");
    return image.base + rva;
}
} // namespace

CategoryVectorCalls admit_category_vector_contract(MemoryView memory, const ImageInfo& game) {
    static_assert(sizeof(std::uintptr_t) == 8 && sizeof(category_layout::Vector) == 24);
    const auto& bindings = game_bindings(game.range).category_vector_contract;
    return {
        reinterpret_cast<decltype(CategoryVectorCalls::grow)>(callable(memory, game.range, bindings.grow_rva)),
        reinterpret_cast<decltype(CategoryVectorCalls::destroy)>(callable(memory, game.range, bindings.destroy_rva)),
    };
}
} // namespace point_zero::native
