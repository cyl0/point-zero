#include "build_bindings.hpp"
#include "list_title_contract.hpp"

#include <stdexcept>

namespace point_zero::native {
ListTitleSite admit_list_title_site(MemoryView memory, const ImageInfo& game) {
    const auto& bindings = game_bindings(game.range).list_title_contract;
    ListTitleSite result{game.range.base + bindings.title.rva,
        std::vector<std::byte>(bindings.title.size),
        game.range.base + bindings.title_background_return_rva,
        bindings.title_width_end_rva - bindings.title_entry_rva};
    if (!contains(game.range.base, game.range.size, result.address, result.body.size()) ||
        !callable_range(memory, result.address, result.body.size()) ||
        !read_bytes(memory, result.address, result.body))
        throw std::runtime_error("list title code unavailable");
    return result;
}
} // namespace point_zero::native
