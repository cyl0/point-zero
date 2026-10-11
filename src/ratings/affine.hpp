#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <string>
#include <vector>

namespace point_zero::ratings::affine {

inline constexpr std::size_t minimum_anchors = 20;
struct Pair { double x, y; };
struct Fit {
    double a = 1, b = 0;
    std::string reason;
    std::size_t overlap = 0;
    int iterations = 0;
};

namespace detail {
inline double median(std::vector<double>& values) {
    const auto middle = values.begin() + values.size() / 2;
    std::nth_element(values.begin(), middle, values.end());
    return values.size() % 2 ? *middle :
        std::midpoint(*std::max_element(values.begin(), middle), *middle);
}
} // namespace detail

// Source x -> reference y. Caller supplies one pair per shared chart, excluding
// censored endpoints; merging, application checks and rounding belong to caller.
// overlap counts finite pairs (including repeated numeric values). iterations
// counts attempted IRLS updates. Invalid fits return raw identity, never offset;
// exhausting the update budget retains a finite positive last fit.
inline Fit fit(std::vector<Pair> pairs) {
    std::erase_if(pairs, [](Pair p) {
        return !std::isfinite(p.x) || !std::isfinite(p.y);
    });
    Fit result;
    result.overlap = pairs.size();
    const auto fallback = [&](const char* reason) {
        result.reason = reason;
        return result;
    };
    if (pairs.size() < minimum_anchors) return fallback("insufficient_overlap");

    double xmin = pairs.front().x, xmax = xmin;
    double ymin = pairs.front().y, ymax = ymin;
    for (const auto p : pairs) {
        xmin = std::min(xmin, p.x); xmax = std::max(xmax, p.x);
        ymin = std::min(ymin, p.y); ymax = std::max(ymax, p.y);
    }
    const double x0 = std::midpoint(xmin, xmax), y0 = std::midpoint(ymin, ymax);
    double xs = 0, ys = 0;
    for (auto& p : pairs) {
        p.x -= x0; p.y -= y0;
        if (!std::isfinite(p.x) || !std::isfinite(p.y)) return fallback("nonfinite_fit");
        xs = std::max(xs, std::abs(p.x)); ys = std::max(ys, std::abs(p.y));
    }
    if (!(xs > 0) || !(ys > 0)) return fallback("inadequate_spread");

    std::vector<double> residuals;
    residuals.reserve(pairs.size());
    for (auto& p : pairs) {
        p.x /= xs; p.y /= ys;
        residuals.push_back(p.y - p.x);
    }
    // The initial slope/median offset, MAD floor, spread and convergence tests
    // all use dimensionless coordinates, independent of either rating unit.
    double a = 1, c = detail::median(residuals);
    std::vector<double> scratch(pairs.size()), weights(pairs.size());
    bool converged = false;
    for (int iteration = 1; iteration <= 20; ++iteration) {
        result.iterations = iteration;
        for (std::size_t i = 0; i < pairs.size(); ++i) {
            residuals[i] = pairs[i].y - std::fma(a, pairs[i].x, c);
            if (!std::isfinite(residuals[i])) return fallback("nonfinite_fit");
        }
        // Preserve residual order for the pair weights; reuse one median buffer.
        std::copy(residuals.begin(), residuals.end(), scratch.begin());
        const double center = detail::median(scratch);
        for (std::size_t i = 0; i < pairs.size(); ++i) {
            scratch[i] = std::abs(residuals[i] - center);
            if (!std::isfinite(scratch[i])) return fallback("nonfinite_fit");
        }
        const double cutoff = 1.345 * std::max(1e-6, 1.4826 * detail::median(scratch));
        if (!std::isfinite(cutoff)) return fallback("nonfinite_fit");
        double sw = 0, sx = 0, sy = 0;
        for (std::size_t i = 0; i < pairs.size(); ++i) {
            const double r = std::abs(residuals[i]);
            const double w = weights[i] = r <= cutoff ? 1 : cutoff / r;
            sw += w; sx += w * pairs[i].x; sy += w * pairs[i].y;
        }
        if (!(sw > 0) || !std::isfinite(sw) || !std::isfinite(sx) || !std::isfinite(sy))
            return fallback("nonfinite_fit");
        const double mx = sx / sw, my = sy / sw;
        double xx = 0, xy = 0;
        for (std::size_t i = 0; i < pairs.size(); ++i) {
            const double dx = pairs[i].x - mx, dy = pairs[i].y - my;
            xx += weights[i] * dx * dx; xy += weights[i] * dx * dy;
        }
        if (!std::isfinite(xx) || !std::isfinite(xy)) return fallback("nonfinite_fit");
        if (xx / sw <= 1e-12) return fallback("inadequate_spread");
        const double next_a = xy / xx, next_c = std::fma(-next_a, mx, my);
        if (!std::isfinite(next_a) || !std::isfinite(next_c)) return fallback("nonfinite_fit");
        converged = std::abs(next_a - a) <= 1e-10 * (1 + std::abs(a)) &&
                    std::abs(next_c - c) <= 1e-10 * (1 + std::abs(c));
        a = next_a; c = next_c;
        if (converged) break;
    }
    // Apply the same validity checks to converged and budget-limited candidates.
    // No raw-unit slope cap. Extreme arithmetic may safely reject a fit.
    const double slope = a * (ys / xs);
    if (!std::isfinite(slope)) return fallback("nonfinite_fit");
    if (!(slope > 0)) return fallback("unsupported_slope");
    const double intercept = std::fma(-slope, x0, std::fma(ys, c, y0));
    if (!std::isfinite(intercept)) return fallback("nonfinite_fit");
    result.a = slope; result.b = intercept;
    result.reason = converged ? "affine" : "affine_at_limit";
    return result;
}

} // namespace point_zero::ratings::affine
