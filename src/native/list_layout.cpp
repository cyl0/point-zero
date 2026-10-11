#include "list_layout.hpp"
#include "list_metrics.hpp"

#include <cmath>

namespace point_zero::native::list_art {
bool placement(int difficulty, bool compact, int tenths, Placement& out) noexcept {
    return difficulty >= 0 && difficulty < 5 && placement(compact, tenths, out);
}

bool placement(bool compact, int tenths, Placement& out) noexcept {
    if (tenths < 10 || tenths > 129) return false;

    const int whole_glyph = tenths / 10;
    const auto whole = detail::whole_sizes[int(compact)][whole_glyph - 1];
    // Approved B84 study(..., mode=2) from list_ux_layout.hpp in
    // 09_outputs/research_v1_playlister51_20261001/candidate/source/src/.
    // Center the native whole in the original 60px window at unit scale.
    const float whole_x = (60.0f - static_cast<float>(whole.width)) / 2.0f;
    const float point_face_x = whole_x + static_cast<float>(whole.width) + 2.0f;

    // API coordinates are offsets from the incoming native draw origin:
    // x = round(sourcepart.x) - 7, y = round(sourcepart.y).
    // The native draw's incoming-origin adjustment is already included here;
    // callers add these offsets directly and must not subtract another 7.
    const auto x = [](float source_x) noexcept {
        return static_cast<int>(std::round(source_x)) - 7;
    };
    // Final SVG cells already contain their effects and transparent bearings:
    // point has 4px padding; fraction has 2px left and 3px top allowance.
    // The point face is 4px high; fraction faces are ordinary 14px / compact 10px.
    // Every returned whole, point and fraction is drawn at scale 1.
    const Placement result{{
        {whole_glyph, x(whole_x), 0},
        {24, x(point_face_x - 4.0f), compact ? 11 : 20},
        {14 + tenths % 10, x(point_face_x + 4.0f), compact ? 6 : 11},
    }};
    out = result;
    return true;
}

bool size(int difficulty, bool compact, int glyph, Size& out) noexcept {
    if (difficulty < 0 || difficulty >= 5) return false;

    Size result;
    if (glyph >= 1 && glyph <= 12)
        result = detail::whole_sizes[int(compact)][glyph - 1];
    else if (glyph >= 14 && glyph <= 23)
        result = detail::fraction_sizes[int(compact)];
    else if (glyph == 24)
        result = detail::point_size;
    else
        return false;
    out = result;
    return true;
}
} // namespace point_zero::native::list_art
