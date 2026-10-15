#include "build_bindings.hpp"
#include "category_summary_contract.hpp"

#include <stdexcept>
#include <utility>

namespace point_zero::native {
CategorySummarySite admit_category_summary_contract(MemoryView memory, const ImageInfo& game) {
    const auto& bindings = game_bindings(game.range).category_summary_contract;
    const auto entry = bindings.category_summary_rva;
    const auto capture = bindings.summary_capture_rva;
    const auto cancel = bindings.summary_continuation_rva;
    if (capture < entry || capture - entry < 5 || cancel < capture || cancel - capture < 5)
        throw std::runtime_error("category summary capture/cancel layout invalid");
    auto body = read_code(memory, game.range, entry, static_cast<std::size_t>(cancel - entry) + 5);
    return {game.range.base + entry, std::move(body), game.range.base + capture, game.range.base + cancel};
}
} // namespace point_zero::native
