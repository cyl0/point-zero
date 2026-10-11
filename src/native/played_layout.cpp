#include "played_layout.hpp"
#include "played_metrics.hpp"
#include "stepup_metrics.hpp"

#include <cmath>
#include <string_view>

namespace point_zero::native::played_art {
namespace {
constexpr int stepup_begin = static_cast<int>(std::size(detail::image_table));
constexpr auto all_images = [] {
    std::array<Image, image_count> result{};
    std::size_t index{};
    for (const auto image : detail::image_table) result[index++] = image;
    for (const auto image : detail::stepup_images) result[index++] = image;
    return result;
}();
static_assert(image_count == std::size(detail::image_table) + std::size(detail::stepup_images));
static_assert(std::size(detail::stepup_images) == 5 * detail::family_images);
}
std::span<const Image> images() noexcept { return all_images; }

Plan make(Surface surface, int difficulty, int tenths) noexcept {
    if ((surface != Surface::confirmation && surface != Surface::hud &&
         surface != Surface::result && surface != Surface::stepup) || difficulty < 0 || difficulty > 4 ||
        tenths < 10 || tenths > 129) return {};

    const int whole = tenths / 10, fraction = tenths % 10;
    if (surface == Surface::stepup) {
        const auto base = stepup_begin + difficulty * detail::family_images;
        const int wi = base + whole - 1, fi = base + 12 + fraction, pi = base + 22;
        const auto& w = detail::stepup_wholes[difficulty * 12 + whole - 1];
        const auto& f = all_images[fi]; const auto& p = all_images[pi];
        // Prepared 30px fraction column and 8px point, separated by 3px.
        // Their crop offsets include the complete native-width feather. The
        // narrow 1 bearing is in the texture; all digits take this same path.
        const float width = w.advance + 44;
        const float left = std::round((difficulty == 2 ? 74.5f : 75.f) - width / 2);
        return {{{{wi, 1, left - w.left, 0},
                  {pi, 1, left + w.advance + 3 + static_cast<float>(p.x), w.baseline + static_cast<float>(p.y)},
                  {fi, 1, left + w.advance + 14 + static_cast<float>(f.x), w.baseline + static_cast<float>(f.y)}}}, 3, width, false};
    }
    const int base = surface == Surface::hud ? detail::hud_begin :
        (surface == Surface::result ? detail::result_begin : 0) +
        difficulty * detail::family_images;
    const int wi = base + (surface == Surface::hud ? whole : whole - 1);
    const int fi = base + (surface == Surface::hud ? fraction : 12 + fraction);
    const int pi = base + (surface == Surface::hud ? 13 : 22);
    const auto& w = detail::image_table[wi];
    const auto& f = detail::image_table[fi];
    const auto& d = detail::image_table[pi];
    const float ww = static_cast<float>(w.width);

    // Ordinary make() in point_zero_played_layout.hpp, retained at
    // 09_outputs/research_v1_playlister51_20261001/candidate/source/src/.
    Plan out;
    if (surface == Surface::confirmation) {
        const auto& fm = detail::confirmation_fraction[fraction];
        const auto& dm = detail::confirmation_point;
        const float baseline = detail::confirmation_baseline[whole - 1];
        constexpr float gap = 2;
        // Retained confirmation art differs from the cup: keep its native
        // dimensions/face baseline, and kern only the measured seven pairs.
        // 7's lower diagonal leaves 16px more point clearance than 6; .7 can
        // give back 2px while preserving the feather channel. Evidence:
        // implementation_v1_playtest_feedback_20261003/spacing-measurements.json.
        const float whole_kern = whole == 7 ? -16.f : 0.f;
        const float fraction_kern = fraction == 7 ? -2.f : 0.f;
        out.width = ww + dm.advance + fm.advance + 2 * gap + whole_kern + fraction_kern;
        const float left = std::round((86 - out.width) / 2);
        out.parts = {{{wi, 1, left, static_cast<float>(w.y)},
                      {pi, 1, left + ww + gap + whole_kern + dm.dx, baseline + dm.dy},
                      {fi, 1, left + ww + gap + dm.advance + gap + whole_kern + fraction_kern + fm.dx,
                       baseline + fm.dy}}};
    } else {
        // Keep native padding/size; kern only HUD 7's open diagonal and .7's
        // lower-left bearing. Result widths and plate behavior stay retained.
        // Limit the combined HUD 7.7 tuck to 7px: its upper bars still need
        // clearance (2px at alpha >=64 in the retained native raster).
        const float whole_kern = surface == Surface::hud && whole == 7 ? -4.f : 0.f;
        const float fraction_kern = surface == Surface::hud && fraction == 7 ? -3.f : 0.f;
        const float dw = static_cast<float>(d.width);
        out.width = ww + dw + static_cast<float>(f.width) + whole_kern + fraction_kern;
        out.extends_result_plate = surface == Surface::result && out.width > 200;
        const float left = surface == Surface::hud ? static_cast<float>(w.x) :
            (out.extends_result_plate ? 200 - out.width : 0);
        const float baseline = surface == Surface::hud ? 19.0f : 21.0f;
        out.parts = {{{wi, 1, left, surface == Surface::hud ? static_cast<float>(w.y) : 0},
                      {pi, 1, left + ww + whole_kern, baseline - static_cast<float>(d.height)},
                      {fi, 1, left + ww + dw + whole_kern + fraction_kern, static_cast<float>(f.y)}}};
    }
    out.count = 3;
    return out;
}

ResultPlate select_result_plate(int players, int sole, int player, const char* leaf) noexcept {
    if (!leaf || player < 0 || player > 1) return {};
    const std::string_view path = leaf;
    if (players == 1 && sole == player && path == "diff_number")
        return {2, 33, 5, "iidxlab_result_plate", false};
    if (players == 2 && player == 0 && path == "diff_number_1p")
        return {3, 38, 6, "iidxlab_result_plate_1p", false};
    if (players == 2 && player == 1 && path == "diff_number_2p")
        return {4, 38, 5, "iidxlab_result_plate_2p", true};
    return {};
}

std::span<const NotesRole> notes_roles() noexcept { return detail::notes; }

namespace {
float extension(float width) noexcept {
    return std::isfinite(width) && width > 200 && width <= 264 ? width - 200 : 0;
}
} // namespace

float number_shift(const ResultPlate& plate, float width) noexcept {
    return plate && plate.mirrored ? extension(width) : 0;
}

float notes_offset(int player, float width) noexcept {
    if (player < 0 || player > 1) return 0;
    return player == 0 ? -extension(width) : extension(width);
}
} // namespace point_zero::native::played_art
