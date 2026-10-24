#include "cup_layout.hpp"
#include "cup_metrics.hpp"

namespace point_zero::native::cup_art {
bool placement(int family, int tenths, Placement& out) noexcept {
    if (family < 0 || family >= 10 || tenths < 0 || tenths > 129) return false;

    const auto& value = detail::families[family].values[tenths];
    Placement result;
    for (unsigned part = 0; part < result.size(); ++part)
        result[part] = {{value.scale[part], value.scale[part]}, {value.x[part], value.y[part]}};
    out = result;
    return true;
}
} // namespace point_zero::native::cup_art
