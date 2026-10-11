#include "song_select.hpp"

namespace point_zero::native {
namespace {
// Reviewed final-datecode ownership from rating_context_bindings and the
// native cup resolver. Group 7 has no supported chart-list owner.
constexpr std::uintptr_t category_data = 0xAAB3B08, category_list = 0xD53278;
constexpr std::array<std::uintptr_t, 12> groups{
    0xCB6D88, 0xCB6B10, 0xCB70B0, 0xCB6BC0, 0xCB6C70, 0xCB6478,
    0xCB63D0, 0, 0xCB69D0, 0xCB6E60, 0xCB6A90, 0xCB0700};

bool image_address(ImageRange image, std::uintptr_t rva, std::uintptr_t& out) noexcept {
    if (rva > UINTPTR_MAX - image.base) return false;
    out = image.base + rva;
    return contains(image.base, image.size, out, sizeof(std::uintptr_t));
}
bool has_vtable(MemoryView memory, ImageRange image, std::uintptr_t owner,
                std::uintptr_t rva) noexcept {
    std::uintptr_t expected{}, actual{};
    return image_address(image, rva, expected) && read_field(memory, owner, 0, actual) && actual == expected;
}
bool selected(MemoryView memory, ImageRange image, Selection& s) noexcept {
    std::uintptr_t root{};
    if (!image_address(image, category_data, root) || !read_field(memory, root, 0, s.data) ||
        !read_field(memory, s.data, 0x175D0, s.list) || !has_vtable(memory, image, s.list, category_list) ||
        !read_field(memory, s.data, 0x2A424, s.style) || s.style < 0 || s.style > 1 ||
        !read_field(memory, s.list, 0x84, s.group_id) || s.group_id < 0 ||
        s.group_id >= static_cast<int>(groups.size()) || !groups[s.group_id]) return false;

    std::uintptr_t head{}, node{};
    if (!read_field(memory, s.list, 0x10, head) || !read_field(memory, head, 0, node)) return false;
    for (unsigned n = 0; node && node != head && n < 64; ++n) {
        int id{};
        std::uintptr_t next{}, group{};
        if (!read_field(memory, node, 0x10, id) || !read_field(memory, node, 0x18, group) ||
            !read_field(memory, node, 0, next)) return false;
        if (id == s.group_id) {
            if (!has_vtable(memory, image, group, groups[s.group_id])) return false;
            s.group = group;
            break;
        }
        node = next;
    }
    std::uintptr_t begin{}, end{};
    const auto style = static_cast<unsigned>(s.style);
    if (!s.group || !read_field(memory, s.group, 0xA8 + 4 * style, s.category_cursor) ||
        s.category_cursor < 0 || !read_field(memory, s.group, 0x78 + 0x18 * style, begin) ||
        !read_field(memory, s.group, 0x80 + 0x18 * style, end) || !begin || end < begin ||
        (end - begin) % 8 ||
        static_cast<std::uintptr_t>(s.category_cursor) >= (end - begin) / 8 ||
        !read_field(memory, begin, 8ULL * s.category_cursor, s.category)) return false;

    std::uint8_t open{};
    int count{}, type{};
    std::uintptr_t bars{}, bars_end{};
    if (!read_field(memory, s.category, 4, open) || open != 1 ||
        !read_field(memory, s.category, 8, count) || count < 1 ||
        !read_field(memory, s.category, 0x14, s.bar_cursor) || s.bar_cursor < 0 || s.bar_cursor >= count ||
        !read_field(memory, s.category, 0x20, bars) || !read_field(memory, s.category, 0x28, bars_end) ||
        !bars || bars_end < bars || bars_end - bars < static_cast<unsigned>(count) * 16ULL ||
        !read_field(memory, bars, 16ULL * s.bar_cursor, type) || type != 1 ||
        !read_field(memory, bars, 16ULL * s.bar_cursor + 8, s.chart) ||
        !read_field(memory, s.chart, 0, s.music) || !read_field(memory, s.chart, 8, s.difficulty) ||
        s.difficulty < 0 || s.difficulty > 4) return false;

    int chart_style{};
    return read_field(memory, s.chart, 0x18, chart_style) && chart_style == s.style &&
           read_field(memory, s.music, 0x67C, s.song_id) && s.song_id > 0;
}
struct CupContext {
    std::uintptr_t first{}, second{};
    int difficulty{};
    std::array<int, 5> values{};
    bool operator==(const CupContext&) const = default;
};
bool cup_context(MemoryView memory, std::uintptr_t owner, CupContext& out) noexcept {
    if (owner < 0x10000 + 0xB0) return false;
    const auto select = owner - 0xB0;
    return read_field(memory, select, 0x7F0, out.first) && read_field(memory, select, 0xC70, out.second) &&
           read_field(memory, owner, 0x64, out.difficulty) && read_field(memory, owner, 0x3C, out.values);
}
} // namespace

bool read_selection(MemoryView memory, ImageRange game, Selection& out) noexcept {
    out = {};
    Selection candidate;
    if (!selected(memory, game, candidate)) return false;
    out = candidate;
    return true;
}

bool read_cup_values(MemoryView memory, ImageRange game, const Catalog& catalog,
                     std::uintptr_t owner, CupValues& out) noexcept {
    out = {};
    Selection first;
    CupContext cups;
    if (!selected(memory, game, first) || !cup_context(memory, owner, cups) ||
        cups.first != first.music || cups.second != first.music || cups.difficulty != first.difficulty) return false;

    CupValues candidate{first.song_id, first.style};
    std::array<std::int8_t, 5> raw{};
    if (!read_field(memory, first.music, 0x3EC + 5ULL * first.style, raw)) return false;
    for (int d = 0; d < 5; ++d) {
        const auto row = catalog.find({first.song_id, first.style, d});
        if (row && row->native_level == raw[d] && cups.values[d] == raw[d]) candidate.charts[d] = row;
    }
    out = candidate;
    return true;
}
} // namespace point_zero::native
