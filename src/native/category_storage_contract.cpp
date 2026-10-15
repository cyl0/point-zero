#include "build_bindings.hpp"
#include "category_storage_contract.hpp"

#include <stdexcept>

namespace point_zero::native {
namespace {
std::uintptr_t callable(MemoryView memory, ImageRange image, std::uint32_t rva) {
    if (rva >= image.size || !contains(image.base, image.size, image.base, image.size) ||
        !callable_range(memory, image.base + rva))
        throw std::runtime_error("category storage function unavailable");
    return image.base + rva;
}
} // namespace

CategoryNativeCalls admit_category_storage_contract(MemoryView memory, const ImageInfo& game) {
    const auto& bindings = game_bindings(game.range).category_storage_contract;
    CategoryNativeCalls result;
    result.construct = reinterpret_cast<decltype(result.construct)>(callable(memory, game.range, bindings.construct_rva));
    result.initialize = reinterpret_cast<decltype(result.initialize)>(callable(memory, game.range, bindings.initialize_rva));
    result.assign_string = reinterpret_cast<decltype(result.assign_string)>(callable(memory, game.range, bindings.assign_string_rva));
    result.insert = reinterpret_cast<decltype(result.insert)>(callable(memory, game.range, bindings.insert_rva));
    result.destroy = reinterpret_cast<decltype(result.destroy)>(callable(memory, game.range, bindings.destroy_rva));
    result.auxiliary_vtable = game.range.base + bindings.auxiliary_vtable_rva;
    if (!contains(game.range.base, game.range.size, result.auxiliary_vtable, sizeof(std::uintptr_t)))
        throw std::runtime_error("category storage auxiliary table outside selected image");
    return result;
}
} // namespace point_zero::native
