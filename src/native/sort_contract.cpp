#include "build_bindings.hpp"
#include "sort_contract.hpp"

#include <stdexcept>

namespace point_zero::native {
SortContract admit_sort_contract(MemoryView memory, const ImageInfo& game) {
    const auto& bindings = game_bindings(game.range).sort_contract;
    const auto base = game.range.base;
    if (!callable_range(memory, base + bindings.mode_rva) ||
        !callable_range(memory, base + bindings.level_rva))
        throw std::runtime_error("sort calls unavailable");
    SortContract result;
    result.calls.mode = reinterpret_cast<decltype(result.calls.mode)>(base + bindings.mode_rva);
    result.calls.level = reinterpret_cast<decltype(result.calls.level)>(base + bindings.level_rva);
    result.hook.caller = base + bindings.sort_return_rva;
    return result;
}
}
