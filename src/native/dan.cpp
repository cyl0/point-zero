#include "dan.hpp"
#include "build_bindings.hpp"
#include "cup_contract.hpp"
#include "list.hpp"
#include "list_layout.hpp"

#include <Windows.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace point_zero::native {
namespace {
constexpr const char* leaves[]{"lv_1st", "lv_2nd", "lv_3rd", "lv_4th"};
// Match list_art::Placement order, independent of the container's authored order.
constexpr const char* parts[]{"whole", "point", "fraction"};
constexpr std::array families{CupRenderer::family, ListRenderer::family};
}

struct DanRenderer::State {
    struct Image { alignas(8) std::array<std::byte, 40> args{}; };
    struct Frame {
        std::uintptr_t scene{};
        std::uint32_t stream{};
        std::array<int, 8> handles{};
        std::size_t count{};
        const CatalogRow* chart{};
        std::array<char, 32> name{};
    };
    struct Owner {
        std::uintptr_t scene{};
        std::uint32_t stream{};
        int mc{};
        std::array<int, 3> children{};
        int value{-1}, difficulty{-1};
        bool restoring{};
    };
    MemoryView memory;
    const Catalog& catalog;
    FeatureState& feature;
    CupAfp afp;
    const std::uint32_t scene_layer_offset;
    int (__fastcall* image_info)(void*, std::uint32_t, const char*){};
    int (__fastcall* arguments)(void*, const void*){};
    int (__fastcall* bind_image)(int, const void*, int){};
    std::array<std::array<Image, 25>, 5> images{};
    // Four rows, with room for their retiring animation siblings/generations.
    std::array<Owner, 32> owners{};
    std::array<std::uintptr_t, 2> slots{};
    std::array<std::uint32_t, 2> packages{};
    std::uint32_t component{};
    DWORD thread{};
    unsigned depth{};
    Frame current;
    bool attempted{}, prepared{};

    explicit State(const AdapterContext& context)
        : memory(context.memory), catalog(context.catalog), feature(context.state),
          afp(admit_cup_calls(memory, context.modules.afp, context.modules.utils)),
          scene_layer_offset(game_bindings(context.modules.game.range).dan_contract.scene_layer_offset) {
        image_info = reinterpret_cast<decltype(image_info)>(resolve_export(memory, context.modules.utils, 71));
        arguments = reinterpret_cast<decltype(arguments)>(resolve_export(memory, context.modules.utils, 77));
        bind_image = reinterpret_cast<decltype(bind_image)>(resolve_export(memory, context.modules.afp, 137));
        if (!image_info || !arguments || !bind_image) throw std::runtime_error("Dan image calls unavailable");
    }
    bool same_thread() const noexcept { return thread && thread == GetCurrentThreadId(); }
    bool live() const {
        for (std::size_t i = 0; i < slots.size(); ++i) {
            int state{};
            std::uint32_t id{};
            if (!read_field(memory, slots[i], 0x510, state) || state != 2 ||
                !read_field(memory, slots[i], 0x514, id) || id != packages[i] ||
                afp.package(families[i].data()) != id) return false;
        }
        return true;
    }
    bool preload(std::uintptr_t primary, std::uintptr_t list) {
        slots = {primary, list};
        for (std::size_t i = 0; i < slots.size(); ++i) {
            std::array<char, 256> name{};
            if (!read_field(memory, slots[i], 8, name) ||
                std::string_view(name.data(), families[i].size()) != families[i] || name[families[i].size()] ||
                !read_field(memory, slots[i], 0x514, packages[i]) ||
                (packages[i] & 0x78000000U) != 0x30000000U) return false;
        }
        if (!live()) return false;
        std::uint64_t info[8]{};
        if (afp.animation_info(info, packages[0], "lab_glyph_bs") ||
            (std::uint32_t(info[3]) & 0x70000000U) != 0x30000000U) return false;
        component = std::uint32_t(info[3]);
        for (int difficulty = 0; difficulty < 5; ++difficulty) for (int glyph = 1; glyph < 25; ++glyph) {
            if (glyph == 13) continue;
            char resource[32]{};
            if (glyph <= 12) std::snprintf(resource, sizeof(resource), "lab_lg_%d_%02d", difficulty, glyph);
            else std::snprintf(resource, sizeof(resource), "pz_lf_%d_%d", difficulty, glyph == 24 ? 13 : glyph - 14);
            alignas(8) std::array<std::byte, 64> image{};
            if (image_info(image.data(), packages[1], resource) ||
                arguments(images[difficulty][glyph].args.data(), image.data())) return false;
        }
        return live();
    }
    bool preload_checked(std::uintptr_t primary, std::uintptr_t list) noexcept {
        __try { return preload(primary, list); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
    void resolve(const DanCapture& captured, std::uintptr_t wrapper, const char* name) {
        if (!prepared || !feature.view().enabled() || !name || wrapper != captured.wrapper ||
            captured.row < 1 || captured.row > 4 || captured.style < 0 || captured.style > 1 ||
            captured.difficulty < 0 || captured.difficulty > 4 || !live()) return;
        Frame frame;
        int song{};
        std::int8_t level{};
        if (!read_field(memory, captured.music, 0x67C, song) || song <= 0 ||
            !read_field(memory, captured.music, 0x3EC + 5ULL * captured.style + captured.difficulty, level) ||
            level != captured.native_level) return;
        frame.chart = catalog.find({song, captured.style, captured.difficulty});
        if (!frame.chart || !frame.chart->assigned || frame.chart->native_level != level) return;
        std::snprintf(frame.name.data(), frame.name.size(), "list_%c%d", "gbyal"[captured.difficulty], level);
        if (std::strcmp(name, frame.name.data())) return;
        std::uintptr_t layer{};
        int first{};
        if (!read_field(memory, captured.scene, scene_layer_offset, layer) ||
            !read_field(memory, layer, 8, frame.stream) || afp.valid(5, frame.stream) < 0 ||
            !read_field(memory, wrapper, 0x200, first) || first != afp.path(frame.stream, leaves[captured.row - 1])) return;
        for (int mc = first; mc >= 0; mc = afp.relative(mc, 6)) {
            if (frame.count == frame.handles.size() || afp.valid(6, mc) < 0) return;
            for (std::size_t i = 0; i < frame.count; ++i) if (frame.handles[i] == mc) return;
            frame.handles[frame.count++] = mc;
        }
        frame.scene = captured.scene;
        current = frame;
    }
    void collect(Owner& owner) {
        if (!owner.mc) return;
        if (afp.valid(5, owner.stream) < 0 || afp.valid(6, owner.mc) < 0) { owner = {}; return; }
        if (!owner.restoring) return;
        for (const int child : owner.children) if (child > 0 && afp.valid(6, child) >= 0) return;
        owner = {};
    }
    bool restore(Owner& owner, int mc, const char* name, int mode, int& result, StockBitmap stock) {
        result = stock(mc, name, mode);
        if (!result) owner.restoring = true;
        return true;
    }
    bool draw(int mc, const char* name, int mode, int& result, StockBitmap stock) {
        Owner* owner{};
        for (auto& candidate : owners) if (candidate.mc == mc) {
            collect(candidate);
            if (candidate.mc) owner = &candidate;
            break;
        }
        const auto fallback = [&] { return owner ? restore(*owner, mc, name, mode, result, stock) : false; };
        if (depth != 1 || !current.chart || !feature.view().enabled() || mode || !name ||
            std::strcmp(name, current.name.data()) || !live()) return fallback();
        bool found{};
        for (std::size_t i = 0; i < current.count; ++i) found |= current.handles[i] == mc;
        if (!found || (owner && (owner->restoring || owner->scene != current.scene ||
                                owner->stream != current.stream))) return fallback();
        list_art::Placement placement;
        const int value = current.chart->effective_tenths, difficulty = current.chart->key.difficulty;
        if (!list_art::placement(false, value, placement)) return fallback();
        if (!owner) {
            for (auto& candidate : owners) {
                collect(candidate);
                if (!candidate.mc) { owner = &candidate; break; }
            }
            if (!owner) return false;
        }
        const bool fresh = !owner->mc;
        const bool changed = fresh || owner->value != value || owner->difficulty != difficulty;
        if (fresh) {
            owner->scene = current.scene; owner->stream = current.stream; owner->mc = mc;
            if (afp.attach(mc, component) || afp.control(mc, 0xF04, 0)) return fallback();
        }
        for (std::size_t i = 0; i < placement.size(); ++i) {
            const int child = afp.path_relative(mc, parts[i]);
            if (child <= 0 || afp.valid(6, child) < 0 ||
                (!fresh && owner->children[i] != child)) return fallback();
            owner->children[i] = child;
            if (!changed) continue;
            const auto& part = placement[i];
            // Dan has 23px between its 46px level cell and the title. Extend
            // the shared list composition 10px left, preserving both margins
            // and the native glyph height without moving stage labels/titles.
            const float position[]{float(part.x) - 10.f, float(part.y)};
            if (bind_image(child, images[difficulty][part.glyph].args.data(), 0) ||
                afp.property(child, 0x1000, reinterpret_cast<std::intptr_t>(position)) ||
                afp.property(child, 0x101E, 1)) return fallback();
        }
        if (!feature.view().enabled()) return fallback();
        owner->value = value; owner->difficulty = difficulty;
        result = 0;
        return true;
    }
};

DanRenderer::DanRenderer(const AdapterContext& context) : state_(std::make_unique<State>(context)) {}
DanRenderer::~DanRenderer() noexcept = default;
bool DanRenderer::preload(std::uintptr_t primary, std::uintptr_t list) noexcept {
    auto& self = *state_;
    if (self.attempted || self.feature.view().phase != FeaturePhase::preparing) return false;
    self.attempted = true; self.thread = GetCurrentThreadId();
    self.prepared = self.preload_checked(primary, list);
    return self.prepared;
}
void DanRenderer::begin(const DanCapture& captured, std::uintptr_t wrapper, const char* name) noexcept {
    auto& self = *state_;
    if (!self.same_thread() || ++self.depth != 1) return;
    self.current = {};
    __try { self.resolve(captured, wrapper, name); }
    __except (EXCEPTION_EXECUTE_HANDLER) { self.current = {}; }
}
void DanRenderer::end() noexcept {
    auto& self = *state_;
    if (self.same_thread() && self.depth && --self.depth == 0) self.current = {};
}
bool DanRenderer::bitmap(int mc, const char* name, int mode, int& result, StockBitmap stock) noexcept {
    auto& self = *state_;
    if (!self.same_thread() || mc <= 0 || !stock) return false;
    __try { return self.draw(mc, name, mode, result, stock); }
    __except (EXCEPTION_EXECUTE_HANDLER) { result = -1; return true; }
}
}
