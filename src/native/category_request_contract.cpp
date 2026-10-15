#include "build_bindings.hpp"
#include "category_request_contract.hpp"

#include <stdexcept>

namespace point_zero::native {
namespace {
std::uintptr_t callable(MemoryView memory, ImageRange image, std::uint32_t rva) {
    if (rva >= image.size || !contains(image.base, image.size, image.base, image.size) ||
        !callable_range(memory, image.base + rva))
        throw std::runtime_error("category request function unavailable");
    return image.base + rva;
}
} // namespace

CategoryRequestCalls admit_category_request_calls(MemoryView memory, const ImageInfo& game) {
    const auto& bindings = game_bindings(game.range).category_request_contract;
    CategoryRequestCalls result;
    result.settings = reinterpret_cast<decltype(result.settings)>(callable(memory, game.range, bindings.settings_rva));
    result.persistent = reinterpret_cast<decltype(result.persistent)>(callable(memory, game.range, bindings.persistent_rva));
    result.store = reinterpret_cast<decltype(result.store)>(callable(memory, game.range, bindings.store_rva));
    result.save = reinterpret_cast<decltype(result.save)>(callable(memory, game.range, bindings.save_rva));
    result.clear = reinterpret_cast<decltype(result.clear)>(callable(memory, game.range, bindings.clear_rva));
    result.group = reinterpret_cast<decltype(result.group)>(callable(memory, game.range, bindings.group_rva));
    result.sound = reinterpret_cast<decltype(result.sound)>(callable(memory, game.range, bindings.sound_rva));
    return result;
}
} // namespace point_zero::native
