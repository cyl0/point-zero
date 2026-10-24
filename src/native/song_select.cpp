#include "song_select.hpp"

namespace point_zero::native {
bool read_cup_binding(MemoryView memory, std::uintptr_t owner, std::uintptr_t music,
                      int style, CupBinding& out) noexcept {
    out = {};
    CupBinding candidate;
    candidate.owner = owner; candidate.music = music; candidate.style = style;
    if (owner < 0x10000 || music < 0x10000 || style < 0 || style > 1 ||
        !read_field(memory, music, 0x67C, candidate.song_id) || candidate.song_id < 1 ||
        !read_field(memory, owner, 0x3C, candidate.levels) ||
        !read_field(memory, owner, 0, candidate.layers)) return false;
    out = candidate;
    return true;
}

bool read_cup_values(MemoryView memory, const Catalog& catalog, const CupBinding& binding,
                     int style, CupValues& out) noexcept {
    out = {};
    CupBinding observed;
    if (!read_cup_binding(memory, binding.owner, binding.music, style, observed) ||
        observed.song_id != binding.song_id || observed.style != binding.style ||
        observed.levels != binding.levels || observed.layers != binding.layers) return false;

    CupValues candidate{binding.song_id, binding.style};
    std::array<std::int8_t, 5> raw{};
    if (!read_field(memory, binding.music, 0x3EC + 5ULL * binding.style, raw)) return false;
    for (int d = 0; d < 5; ++d) {
        const auto row = catalog.find({binding.song_id, binding.style, d});
        if (row && row->native_level == raw[d] && binding.levels[d] == raw[d]) candidate.charts[d] = row;
    }
    out = candidate;
    return true;
}
} // namespace point_zero::native
