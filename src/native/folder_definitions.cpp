#include "folder_definitions.hpp"
#include "category_bank.hpp"
#include "folder_metrics.hpp"

namespace point_zero::native {
namespace {
constexpr const char* parent_rows[]{
    "so_dif1", "so_dif2", "so_dif3", "so_dif4", "so_dif5", "so_dif6",
    "so_dif7", "so_dif8", "so_dif9", "so_dif10", "so_dif11", "so_dif12",
};
constexpr const char* parent_hovers[]{
    "ss_dif1", "ss_dif2", "ss_dif3", "ss_dif4", "ss_dif5", "ss_dif6",
    "ss_dif7", "ss_dif8", "ss_dif9", "ss_dif10", "ss_dif11", "ss_dif12",
};
static_assert(CategoryBank::per_style == 144 && folder_art::images.size() == 264);
} // namespace

std::array<CategoryMetadataEntry, 144> folder_definitions() noexcept {
    std::array<CategoryMetadataEntry, 144> result{};
    for (std::size_t n = 0; n < result.size(); ++n) {
        const auto level = 11 - n / 12;
        const auto part = n % 12;
        auto& entry = result[n];
        entry.id = CategoryBank::first_id + static_cast<int>(n);
        auto& definition = entry.definition;
        definition.default_sort = part == 0 ? 0 : 1; // Children start in native TITLE order.
        definition.difficulty_switchable = 1;
        definition.difficulty_locked = 1;
        definition.hover_header = "ss_difficulty";
        definition.ticker = "";
        if (part == 0) {
            definition.hover_subheader = parent_hovers[level];
            definition.bar_text = parent_rows[level];
            definition.bar_background = "so_b_difficulty";
        } else {
            const auto image = (level * 11 + part - 1) * 2;
            definition.bar_text = folder_art::images[image].name;
            definition.hover_subheader = folder_art::images[image + 1].name;
            definition.bar_background = "so_b_djlevel"; // Existing native amber plate.
        }
    }
    return result;
}
} // namespace point_zero::native
