#include "build_bindings.hpp"
#include "list_hook_contract.hpp"

#include <array>
#include <cstring>
#include <stdexcept>

namespace point_zero::native {
ListHookSite admit_list_hook_site(MemoryView memory, const ImageInfo& game) {
    const auto base = game.range.base;
    const auto& bindings = game_bindings(game.range).list_hook_contract;
    ListHookSite result{};
    result.callers = {base + bindings.background_return_rva, base + bindings.p1_return_rva,
        base + bindings.p2_return_rva, base + bindings.solo_return_rva};
    for (const auto caller : result.callers) {
        std::array<std::byte, 5> bytes{};
        std::int32_t displacement{};
        if (caller < 5 || !contains(base, game.range.size, caller - 5, bytes.size()) ||
            !callable_range(memory, caller - 5, bytes.size()) || !read_bytes(memory, caller - 5, bytes) ||
            bytes[0] != std::byte{0xe8}) throw std::runtime_error("list draw CALL unavailable");
        std::memcpy(&displacement, bytes.data() + 1, sizeof(displacement));
        if (!callable_range(memory, static_cast<std::uintptr_t>(static_cast<std::int64_t>(caller) + displacement)))
            throw std::runtime_error("list draw target unavailable");
    }
    return result;
}
} // namespace point_zero::native
