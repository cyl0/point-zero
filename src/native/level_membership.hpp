#pragma once

#include "level_population.hpp"

namespace point_zero::native {
// Prepared membership only. Native categories, publication, summaries, sorting,
// navigation and rollback belong to the browsing owner. Parent and ALL use the
// same full population; empty decimal spans are omitted by the group builder.
class LevelMembership final {
public:
    // One attempt; allocate the required member capacity before building.
    // A chart occupies ALL plus a decimal child only when explicitly assigned.
    void prepare(std::size_t capacity = 4 * LevelPopulation::style_limit);
    // Input is the strictly Key-ordered, stable LevelPopulation snapshot, not
    // the registered catalogue. It must not alias this object's member storage.
    // Copies members without native calls, allocation or mutation of the input.
    // A failed build invalidates every output span. Recapture/rebuild on return;
    // these values confer no continuing eligibility or native-owner lifetime.
    bool build(std::span<const LevelChart>) noexcept;
    bool valid() const noexcept { return valid_; }
    std::span<const LevelChart> all(int style, int level) const noexcept;
    std::span<const LevelChart> decimal(int style, int level, int tenth) const noexcept;
private:
    struct Range { std::size_t begin{}, count{}; };
    static constexpr std::size_t bucket_count = 2 * 12 * 11;
    std::array<Range, bucket_count> ranges_{};
    std::vector<LevelChart> members_;
    bool attempted_{}, prepared_{}, valid_{};
    static std::size_t bucket(int style, int level, int part) noexcept;
    std::span<const LevelChart> view(int style, int level, int part) const noexcept;
};
} // namespace point_zero::native
