#include "list.hpp"

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace point_zero::native {
namespace {
constexpr std::uintptr_t pool_rva = 0xB3DF3E0, sprite_vtable = 0xE6C810;
constexpr std::uintptr_t pool_count = 0x4C0008, pool_slots = 0x260008, sprite_size = 0x260;
constexpr const char* stock_backgrounds[]{"listb", "listb_secret", "listb_lightning"};
constexpr const char* private_backgrounds[]{"pz_ux_2_listb", "pz_ux_2_listb_secret", "pz_ux_2_listb_lightning"};
bool background_has_custom(MemoryView memory, const Catalog& catalog, ListChartCalls calls,
                           std::uintptr_t owner) noexcept {
    // The background precedes the labels and is shared by both players. Resolve
    // this draw's owner; never carry a decision across rows or frame-pool reuse.
    __try {
        const int players = calls.player_count(), style = calls.style();
        if ((players != 1 && players != 2) || style < 0 || style > 1) return false;
        const int first = players == 1 ? calls.active_player() : 0;
        if (first < 0 || first > 1) return false;
        struct Row {
            std::uintptr_t music{};
            std::array<int, 2> difficulties{};
            int style{}, song{};
        } row;
        const auto read_row = [&](Row& out) {
            return read_field(memory, owner, 0, out.music) && read_field(memory, owner, 8, out.difficulties) &&
                read_field(memory, owner, 0x18, out.style) && read_field(memory, out.music, 0x67C, out.song);
        };
        if (!read_row(row) || row.style != style || row.song <= 0) return false;
        bool custom{};
        for (int player = first; player < (players == 1 ? first + 1 : 2); ++player) {
            const int difficulty = row.difficulties[player];
            if (difficulty < 0 || difficulty > 4) return false;
            const auto* chart = catalog.find({row.song, style, difficulty});
            if (!chart) continue;
            custom = custom || chart->assigned;
        }
        return custom;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool coordinate(int origin, int delta, int& value) noexcept {
    const auto sum = static_cast<std::int64_t>(origin) + delta;
    if (sum < (std::numeric_limits<int>::min)() || sum > (std::numeric_limits<int>::max)()) return false;
    value = static_cast<int>(sum); return true;
}
} // namespace

ListRenderer::ListRenderer(const AdapterContext& context, ListNativeCalls calls)
    : game_(context.modules.game.range), memory_(context.memory), catalog_(context.catalog),
      state_(context.state), host_(context.host), calls_(calls) {
    if (!calls.chart.player_count || !calls.chart.style || !calls.chart.active_player ||
        !calls.valid || !calls.package ||
        !calls.image_info || !calls.arguments || !calls.creation_level || !calls.set_creation_level ||
        !calls.emit || !calls.visible)
        throw std::invalid_argument("incomplete admitted list native calls");
}
void ListRenderer::fault() noexcept {
    if (warned_.test_and_set(std::memory_order_relaxed) || !host_.log) return;
    constexpr std::string_view message = "list drawing skipped a native operation; later callbacks remain enabled";
    try { host_.log(host_.context, PZ_LOG_WARNING,
        {reinterpret_cast<const std::uint8_t*>(message.data()), message.size()}); } catch (...) {}
}
bool ListRenderer::live() const {
    int state{}; std::uint32_t id{};
    return thread_ == GetCurrentThreadId() && read_field(memory_, slot_, 0x510, state) && state == 2 &&
        read_field(memory_, slot_, 0x514, id) && id == package_ && calls_.package(family.data()) == id;
}
bool ListRenderer::image(Image& out, const char* name) {
    alignas(8) std::array<std::byte, 64> info{};
    if (calls_.image_info(info.data(), package_, name)) return false;
    return calls_.arguments(out.args.data(), info.data()) == 0;
}
bool ListRenderer::preload_impl(std::uintptr_t slot) {
    if (attempted_ || state_.view().phase != FeaturePhase::preparing) return false;
    attempted_ = true; thread_ = GetCurrentThreadId(); slot_ = slot;
    std::array<char, 256> name{};
    if (!read_field(memory_, slot, 8, name) || std::string_view(name.data(), family.size()) != family ||
        name[family.size()] != '\0' || !read_field(memory_, slot, 0x514, package_) ||
        (package_ & 0x78000000U) != 0x30000000U || !live()) return false;
    for (int difficulty = 0; difficulty < 5; ++difficulty) for (int compact = 0; compact < 2; ++compact) {
        for (int glyph = 1; glyph < 25; ++glyph) {
            if (glyph == 13) continue; // The old stock point is not used by B84.
            char resource[48]{};
            if (glyph <= 12) std::snprintf(resource, sizeof(resource), "lab_lg_%d_%02d%s",
                                         difficulty, glyph, compact ? "s" : "");
            else std::snprintf(resource, sizeof(resource), compact ? "pz_lfc_%d_%d" : "pz_lf_%d_%d",
                               difficulty, glyph == 24 ? 13 : glyph - 14);
            if (!image(images_[difficulty][compact][glyph], resource)) return false;
        }
    }
    for (std::size_t i = 0; i < backgrounds_.size(); ++i)
        if (!image(backgrounds_[i], private_backgrounds[i])) return false;
    ready_ = live(); return ready_;
}
bool ListRenderer::preload(std::uintptr_t slot) noexcept {
    __try { return preload_impl(slot); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool ListRenderer::hide(std::span<const Submitted> submitted, int expected_pool_count) {
    int count{}; std::uintptr_t method{};
    if (!read_field(memory_, game_.base + pool_rva, pool_count, count) || count != expected_pool_count ||
        !read_field(memory_, game_.base + sprite_vtable, 0x28, method) ||
        method != reinterpret_cast<std::uintptr_t>(calls_.visible)) return false;
    // Validate the whole known set before asking the native visibility method
    // to hide any part. Never act on a recycled handle or an unexpected pointer.
    for (const auto& part : submitted) {
        std::uintptr_t vt{}; std::uint32_t stream{}; int root{};
        if (!read_field(memory_, part.sprite, 0, vt) || vt != game_.base + sprite_vtable ||
            !read_field(memory_, part.sprite, 8, stream) || stream != part.stream ||
            !read_field(memory_, part.sprite, 0x210, root) || root != part.root ||
            calls_.valid(5, stream) < 0 || calls_.valid(6, root) < 0) return false;
    }
    for (const auto& part : submitted) if (calls_.visible(part.sprite, 0)) return false;
    return read_field(memory_, game_.base + pool_rva, pool_count, count) && count == expected_pool_count;
}
bool ListRenderer::emit_parts(const ListDraw& request, const Image* const* images,
                              const list_art::Part* positions, int count, int before, std::uintptr_t& result) {
    std::array<Submitted, 3> submitted{};
    int owned{}; bool correct = true;
    result = 0;
    for (int i = 0; i < count; ++i) {
        // A native callback may change this shared AFP setting. Do not submit
        // subsequent parts under a level we no longer own.
        if (calls_.creation_level() != 0) { correct = false; break; }
        int x{}, y{};
        // All coordinates were checked before the first native submission.
        (void)coordinate(request.x, positions[i].x, x);
        (void)coordinate(request.y, positions[i].y, y);
        const auto sprite = calls_.emit(request.manager, images[i]->args.data(), x, y,
                                        request.alpha, request.layer, request.flags);
        const auto expected = request.manager + pool_slots + sprite_size * static_cast<unsigned>(before + i);
        std::uintptr_t vt{}; std::uint32_t stream{}; int root{}, current_count{};
        if (sprite != expected || !read_field(memory_, sprite, 0, vt) || vt != game_.base + sprite_vtable ||
            !read_field(memory_, sprite, 8, stream) || !stream || !read_field(memory_, sprite, 0x210, root) || root < 0 ||
            !read_field(memory_, request.manager, pool_count, current_count) || current_count != before + i + 1 ||
            calls_.valid(5, stream) < 0 || calls_.valid(6, root) < 0) { correct = false; break; }
        submitted[owned++] = {sprite, stream, root};
        if (!result) result = sprite;
        // The native emitter initializes every reused sprite to unit scale.
    }
    int after{};
    correct = correct && owned == count && read_field(memory_, request.manager, pool_count, after) &&
        after == before + count && calls_.creation_level() == 0 && live();
    if (correct) return true;
    fault();
    // Native frame-pool entries are never deleted or rewound. If every possible
    // submission is accounted for and can be hidden, the caller may draw stock
    // in the reserved extra slot. Unknown submission remains a handled failure.
    const bool clean = hide(std::span(submitted).first(static_cast<std::size_t>(owned)), before + owned);
    result = 0;
    return !clean;
}
bool ListRenderer::restore_level(int previous) noexcept {
    __try {
        const int current = calls_.creation_level();
        if (current == previous) return true;
        if (current != 0) return false; // Do not overwrite an unowned change.
        return calls_.set_creation_level(previous) == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool ListRenderer::submit(const ListDraw& request, const Image* const* images,
                          const list_art::Part* positions, int count, int before, std::uintptr_t& result) {
    int previous{-1}; bool attempted{}, restored{true}, handled{};
    __try {
        previous = calls_.creation_level();
        if (previous < 0 || previous > 127) { fault(); return false; }
        attempted = true;
        if (calls_.set_creation_level(0)) fault();
        else handled = emit_parts(request, images, positions, count, before, result);
    } __finally {
        if (attempted) restored = restore_level(previous);
    }
    if (!restored) { fault(); result = 0; return true; }
    return handled;
}
bool ListRenderer::draw_impl(const ListDraw& request, std::uintptr_t& result) {
    if (!request.name || request.manager != game_.base + pool_rva) return false;
    const auto ret = request.return_address;
    const bool background = ret == game_.base + 0x5F7E62;
    if (!background && ret != game_.base + 0x5F865C && ret != game_.base + 0x5F86AB &&
        ret != game_.base + 0x5F876D) return false;
    std::array<const Image*, 3> images{};
    list_art::Placement placement{};
    int count{};
    if (background) {
        for (int i = 0; i < 3; ++i) {
            const auto length = std::strlen(stock_backgrounds[i]);
            if (!std::strncmp(request.name, stock_backgrounds[i], length + 1)) {
                images[0] = &backgrounds_[i]; count = 1; break;
            }
        }
        if (!count || !background_has_custom(memory_, catalog_, calls_.chart, request.owner)) return false;
    } else {
        ListChart chart;
        if (!read_list_chart(memory_, game_, catalog_, calls_.chart, request.owner, ret, request.name, chart) ||
            !chart.chart || !chart.chart->assigned ||
            !list_art::placement(chart.compact, chart.chart->effective_tenths, placement)) return false;
        for (int i = 0; i < 3; ++i)
            images[i] = &images_[chart.chart->key.difficulty][int(chart.compact)][placement[i].glyph];
        count = 3;
    }
    if (!state_.view().enabled()) return false;
    if (!ready_ || !live()) { fault(); return false; }
    int before{};
    if (!read_field(memory_, request.manager, pool_count, before) || before < 0 || before > 4096 - count - 1) return false;
    for (int i = 0; i < count; ++i) {
        int x{}, y{};
        if (!coordinate(request.x, placement[i].x, x) || !coordinate(request.y, placement[i].y, y)) return false;
    }
    return submit(request, images.data(), placement.data(), count, before, result);
}
bool ListRenderer::draw(const ListDraw& request, std::uintptr_t& result) noexcept {
    if (!thread_ || thread_ != GetCurrentThreadId() || drawing_ || !state_.view().enabled()) return false;
    __try {
        drawing_ = true;
        __try { return draw_impl(request, result); }
        __finally { drawing_ = false; }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        fault(); result = 0;
        // Native submission may already have happened. Never draw twice after
        // an uncontained fault; backing remains retained for native frame reuse.
        return true;
    }
}
ListHook::Callback ListRenderer::callback() noexcept {
    return [](void* context, std::uintptr_t owner, std::uintptr_t return_address,
              std::uintptr_t manager, const char* name, int x, int y,
              std::uint32_t alpha, std::uint32_t layer, std::uint32_t flags,
              std::uintptr_t& result) noexcept {
        return static_cast<ListRenderer*>(context)->draw(
            {owner, return_address, manager, name, x, y, alpha, layer, flags}, result);
    };
}
bool ListRenderer::title_permitted(std::uintptr_t owner) noexcept {
    if (!thread_ || thread_ != GetCurrentThreadId() || !state_.view().enabled()) return false;
    __try {
        if (!background_has_custom(memory_, catalog_, calls_.chart, owner)) return false;
        if (!ready_ || !live()) { fault(); return false; }
        return state_.view().enabled();
    } __except (EXCEPTION_EXECUTE_HANDLER) { fault(); return false; }
}
ListTitleHook::Permit ListRenderer::title_callback() noexcept {
    return [](void* context, std::uintptr_t owner) noexcept {
        return static_cast<ListRenderer*>(context)->title_permitted(owner);
    };
}
} // namespace point_zero::native
