#include "build_bindings.hpp"
#include "filter_contract.hpp"

#include <stdexcept>

namespace point_zero::native {
namespace {
std::uintptr_t callable(MemoryView memory, ImageRange image, std::uint32_t rva) {
    if (rva >= image.size || !contains(image.base, image.size, image.base, image.size) ||
        !callable_range(memory, image.base + rva))
        throw std::runtime_error("filter function unavailable");
    return image.base + rva;
}
} // namespace

FilterContract admit_filter_contract(MemoryView memory, const ImageInfo& game) {
    const auto& bindings = game_bindings(game.range).filter_contract;
    FilterContract result;
    for (const auto pin : bindings.code_pins) {
        FilterSite* site = pin.rva == bindings.filter_apply_rva ? &result.sites.apply :
                           pin.rva == bindings.filter_level_rva ? &result.sites.level :
                           pin.rva == bindings.filter_candidate_rva ? &result.sites.candidate : nullptr;
        if (site) *site = {game.range.base + pin.rva, read_code(memory, game.range, pin.rva, pin.size)};
    }
    result.sites.apply_caller = game.range.base + bindings.filter_apply_caller_rva;
    result.sites.level_caller = game.range.base + bindings.sites_level_caller_rva;
    result.mode = reinterpret_cast<decltype(result.mode)>(callable(memory, game.range, bindings.mode_rva));
    result.rebuild = reinterpret_cast<decltype(result.rebuild)>(callable(memory, game.range, bindings.rebuild_rva));
    return result;
}
} // namespace point_zero::native
