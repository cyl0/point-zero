#include "level_membership.hpp"

#include <stdexcept>

namespace point_zero::native {
void LevelMembership::prepare(std::size_t capacity) {
    if (attempted_) throw std::runtime_error("LEVEL membership preparation already attempted");
    attempted_ = true;
    if (capacity > 4 * LevelPopulation::style_limit)
        throw std::invalid_argument("LEVEL membership capacity exceeds population limit");
    members_.resize(capacity);
    prepared_ = true;
}
std::size_t LevelMembership::bucket(int style, int level, int part) noexcept {
    return (static_cast<std::size_t>(style) * 12 + level - 1) * 11 + part;
}
bool LevelMembership::build(std::span<const LevelChart> input) noexcept {
    valid_ = false;
    ranges_.fill({});
    if (!prepared_ || input.size() > 2 * LevelPopulation::style_limit) return false;
    std::array<std::size_t, 2> styles{};
    for (std::size_t i = 0; i < input.size(); ++i) {
        const auto& c = input[i];
        if (c.music < 0x10000 || (c.music & 7) || c.key.song <= 0 ||
            c.key.style < 0 || c.key.style > 1 || c.key.difficulty < 0 || c.key.difficulty > 4 ||
            c.native_level < 1 || c.native_level > 12 || c.effective_tenths < 10 || c.effective_tenths > 129 ||
            (!c.assigned && c.effective_tenths != c.native_level * 10) ||
            (i && !(input[i - 1].key < c.key)) || ++styles[c.key.style] > LevelPopulation::style_limit) return false;
        const int level = c.effective_tenths / 10;
        ++ranges_[bucket(c.key.style, level, 0)].count;
        if (c.assigned) ++ranges_[bucket(c.key.style, level, 1 + c.effective_tenths % 10)].count;
    }
    std::array<std::size_t, bucket_count> next{};
    std::size_t used{};
    for (std::size_t b = 0; b < ranges_.size(); ++b) {
        ranges_[b].begin = next[b] = used;
        used += ranges_[b].count;
    }
    if (used > members_.size()) return false;
    for (const auto& c : input) {
        const int level = c.effective_tenths / 10;
        members_[next[bucket(c.key.style, level, 0)]++] = c;
        if (c.assigned) members_[next[bucket(c.key.style, level, 1 + c.effective_tenths % 10)]++] = c;
    }
    valid_ = true;
    return true;
}
std::span<const LevelChart> LevelMembership::view(int style, int level, int part) const noexcept {
    if (!valid_ || style < 0 || style > 1 || level < 1 || level > 12 || part < 0 || part > 10) return {};
    const auto& range = ranges_[bucket(style, level, part)];
    return std::span(members_).subspan(range.begin, range.count);
}
std::span<const LevelChart> LevelMembership::all(int style, int level) const noexcept {
    return view(style, level, 0);
}
std::span<const LevelChart> LevelMembership::decimal(int style, int level, int tenth) const noexcept {
    return tenth < 0 || tenth > 9 ? std::span<const LevelChart>{} : view(style, level, 1 + tenth);
}
} // namespace point_zero::native
