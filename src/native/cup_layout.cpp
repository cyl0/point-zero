#include "cup_layout.hpp"
#include "cup_metrics.hpp"

namespace point_zero::native::cup_art {
bool placement(int family, int tenths, Placement& out) noexcept {
    if (family < 0 || family >= 6 || tenths < 0 || tenths > 129) return false;

    const auto& geometry = detail::geometry[detail::family_geometry[family]];
    const auto& whole = geometry.whole[tenths / 10];
    const auto& optical = geometry.optical[tenths];

    // Port of native_cup_layout.hpp in
    // 09_outputs/research_v1_playlister51_20261001/candidate/source/src/.
    // prepare(family, 129) fixes the baseline for the entire count-up at scale 1.
    const float baseline = (geometry.canvas_height + geometry.maximum_height) / 2;
    const float origin = (geometry.canvas_width - optical.width) / 2;
    // Restore the packed native whole quad to its original art dimensions.
    const float scale = whole.native_width / whole.source_width;

    Placement result;
    result[0] = {{scale, scale},
                 {origin - whole.source_x * scale,
                  baseline - whole.native_height - whole.source_y * scale}};
    // Dedicated SVG fraction/point quads are already at their final pixel size.
    result[1].position = {origin + optical.fraction_x, baseline + optical.fraction_y};
    result[2].position = {origin + optical.dot_x - detail::point_padding,
                          baseline + optical.dot_y - detail::point_padding};
    out = result;
    return true;
}
} // namespace point_zero::native::cup_art
