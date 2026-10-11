#include "list_chart.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <array>

namespace point_zero::native {
namespace {
static_assert(sizeof(std::uintptr_t) == 8);
// Reviewed LDJ2026081900 final branch of native_resource_bindings. These are
// complete bitmap-call return RVAs, not function entries or low address bits.
constexpr std::uintptr_t p1_site = 0x5F865C, p2_site = 0x5F86AB, solo_site = 0x5F876D;
constexpr std::uintptr_t normal_names = 0x105D0F0, compact_names = 0x105D300;

struct Owner {
    std::uintptr_t music{};
    std::array<int, 2> difficulties{};
    int style{}, song{};
};
bool read_owner(MemoryView memory, std::uintptr_t owner, Owner& out) noexcept {
    return read_field(memory, owner, 0, out.music) &&
        read_field(memory, owner, 8, out.difficulties) &&
        read_field(memory, owner, 0x18, out.style) && out.style >= 0 && out.style <= 1 &&
        read_field(memory, out.music, 0x67C, out.song) && out.song > 0;
}

struct Mode {
    int count{}, style{}, active{-1};
};
bool read_mode(ListChartCalls calls, Mode& out) noexcept {
    out.count = calls.player_count();
    if (out.count != 1 && out.count != 2) return false;
    out.style = calls.style();
    if (out.style < 0 || out.style > 1) return false;
    // Paired dispatch is owned by the return site, never by active_player.
    if (out.count == 1) {
        if (!calls.active_player) return false;
        out.active = calls.active_player();
        if (out.active < 0 || out.active > 1) return false;
    }
    return true;
}


bool resolve(MemoryView memory, ImageRange game, const Catalog& catalog,
             ListChartCalls calls, std::uintptr_t owner, std::uintptr_t return_address,
             const char* incoming, ListChart& out) noexcept {
    if (!memory.read || !calls.player_count || !calls.style || game.base < 0x10000 ||
        !contains(game.base, game.size, return_address, 1)) return false;
    const auto site = return_address - game.base;
    if (site != solo_site && site != p1_site && site != p2_site) return false;

    Owner first;
    Mode mode;
    if (!incoming || !read_owner(memory, owner, first) ||
        !read_mode(calls, mode) || first.style != mode.style) return false;

    int player{};
    bool compact{};
    if (mode.count == 1) {
        if (site != solo_site) return false;
        player = mode.active;
    } else {
        for (const auto difficulty : first.difficulties)
            if (difficulty < 0 || difficulty > 4) return false;
        compact = first.difficulties[0] != first.difficulties[1];
        if (compact) {
            if (site == p1_site) player = 0;
            else if (site == p2_site) player = 1;
            else return false;
        } else if (site != p2_site) return false; // Equal pair emits one normal label for P1.
    }
    const auto difficulty = first.difficulties[player];
    if (difficulty < 0 || difficulty > 4) return false;
    const auto* row = catalog.find({first.song, mode.style, difficulty});
    if (!row) return false;

    const auto name_rva = (compact ? compact_names : normal_names) +
        sizeof(std::uintptr_t) * static_cast<std::size_t>(13 * difficulty + row->native_level);
    if (name_rva > UINTPTR_MAX - game.base) return false;
    const auto name_slot = game.base + name_rva;
    std::uintptr_t expected{};
    if (!contains(game.base, game.size, name_slot, sizeof(expected)) ||
        !read_field(memory, name_slot, 0, expected) || expected < 0x10000 ||
        expected != reinterpret_cast<std::uintptr_t>(incoming)) return false;

    // This callback and the chart table run on the native game thread. The
    // immutable catalog and current owner identify the row without a second
    // getter call or a second snapshot of every field.
    // Assignment presence belongs to the immutable row. A valid native-only
    // chart is still a successful identity observation, including native DPB.
    out = {compact, row};
    return true;
}
} // namespace

bool read_list_chart(MemoryView memory, ImageRange game, const Catalog& catalog,
                     ListChartCalls calls, std::uintptr_t owner, std::uintptr_t return_address,
                     const char* incoming, ListChart& out) noexcept {
    out = {};
    __try {
        ListChart candidate;
        if (!resolve(memory, game, catalog, calls, owner, return_address, incoming, candidate)) return false;
        out = candidate;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out = {};
        return false;
    }
}
} // namespace point_zero::native
