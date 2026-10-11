#pragma once

#include "ratings/ratings.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace point_zero::native {

// Values copied by the admitted live catalogue adapter. Registration says
// nothing about current eligibility, unlocks, scores or menu membership.
struct RegisteredChart {
    ratings::Key key;
    int native_level{};
};

struct CatalogRow {
    ratings::Key key;
    int native_level{};
    int effective_tenths{};
    // Distinguishes explicit 12.0 from native 12 even when both have 120 tenths.
    bool assigned{};
};

class Catalog {
public:
    std::span<const CatalogRow> rows() const noexcept { return rows_; }
    // Binary search; no allocation, parsing or access to native/prepared state.
    // Returned pointers/spans remain valid until this catalogue is replaced,
    // moved from or destroyed.
    const CatalogRow* find(ratings::Key key) const noexcept;

private:
    std::vector<CatalogRow> rows_;

    friend Catalog bind_catalog(std::span<const RegisteredChart>, const ratings::Snapshot&);
};

// Accepts any chart order; result rows are strictly ordered by Key.
// Native keys permit styles 0/1 and difficulties 0..4 (including native DPB),
// positive int32 song IDs and levels 1..12. Prepared keys must follow the public
// ratings format (no DPB), be strictly ordered/unique, and carry finite values,
// tenths 10..129 and an in-range source index. Numerical consistency/precedence
// belongs to ratings::prepare; this function never normalizes or fits again.
//
// Throws std::invalid_argument for invalid input, or propagates allocation
// failure. Inputs are never modified; no partial result is published. Build a
// replacement with `catalog = bind_catalog(charts, prepared)` for strong failure
// safety. No native pointers or borrowed input storage are retained. Keep the
// immutable Snapshot when supplied/adapted values, source metadata or entry
// provenance are needed; those remain available by Key in the Snapshot. Row
// lookup does not depend on the Snapshot's lifetime. This is an internal C++ API.
Catalog bind_catalog(std::span<const RegisteredChart> charts,
                     const ratings::Snapshot& prepared);

} // namespace point_zero::native
