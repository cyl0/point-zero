#include "catalog.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace point_zero::native {
namespace {

bool valid_native_key(ratings::Key key) noexcept {
    return key.song > 0 && key.style >= 0 && key.style <= 1 &&
           key.difficulty >= 0 && key.difficulty <= 4;
}

} // namespace

const CatalogRow* Catalog::find(ratings::Key key) const noexcept {
    const auto it = std::lower_bound(rows_.begin(), rows_.end(), key,
        [](const CatalogRow& row, ratings::Key sought) { return row.key < sought; });
    return it != rows_.end() && it->key == key ? &*it : nullptr;
}

Catalog bind_catalog(std::span<const RegisteredChart> charts,
                     const ratings::Snapshot& prepared) {
    // Validate even assignments without registered charts. Never repair/sort/
    // prune the preparation owner's snapshot or change its provenance.
    for (std::size_t i = 0; i < prepared.assignments.size(); ++i) {
        const auto& assignment = prepared.assignments[i];
        const auto key = assignment.key;
        if (!valid_native_key(key) || (key.style == 1 && key.difficulty == 0))
            throw std::invalid_argument("catalog: invalid prepared chart key");
        if (i != 0 && !(prepared.assignments[i - 1].key < key))
            throw std::invalid_argument("catalog: prepared keys must be strictly ordered and unique");
        if (!std::isfinite(assignment.supplied) || !std::isfinite(assignment.adapted) ||
            assignment.tenths < 10 || assignment.tenths > 129)
            throw std::invalid_argument("catalog: invalid prepared rating value");
        if (assignment.source < 0 ||
            static_cast<std::size_t>(assignment.source) >= prepared.sources.size())
            throw std::invalid_argument("catalog: invalid prepared source index");
    }

    Catalog result;
    result.rows_.reserve(charts.size());
    for (const auto& chart : charts) {
        if (!valid_native_key(chart.key))
            throw std::invalid_argument("catalog: invalid registered chart key");
        if (chart.native_level < 1 || chart.native_level > 12)
            throw std::invalid_argument("catalog: native level must be in 1..12");
        result.rows_.push_back({chart.key, chart.native_level, chart.native_level * 10, false});
    }
    std::sort(result.rows_.begin(), result.rows_.end(),
        [](const CatalogRow& left, const CatalogRow& right) { return left.key < right.key; });
    for (std::size_t i = 1; i < result.rows_.size(); ++i) {
        if (result.rows_[i - 1].key == result.rows_[i].key)
            throw std::invalid_argument("catalog: duplicate registered chart key");
    }

    auto row = result.rows_.begin();
    for (const auto& assignment : prepared.assignments) {
        while (row != result.rows_.end() && row->key < assignment.key) ++row;
        if (row != result.rows_.end() && row->key == assignment.key) {
            row->effective_tenths = assignment.tenths;
            row->assigned = true;
        }
    }
    return result;
}

} // namespace point_zero::native
