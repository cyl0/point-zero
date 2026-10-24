#include "assistant.hpp"
#include "cup_contract.hpp"

#include <Windows.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace point_zero::native {
namespace {
// Accepted pz34_assistant.ifs geometry: 40x18 labels, full native glyph height.
constexpr float native_x[]{476.f, 498.f, 520.f, 542.f, 564.f};
constexpr float decimal_x[]{395.f, 435.f, 475.f, 515.f, 555.f};
constexpr float origin_shift = 9.f, centre_y = 17.f;
constexpr char letters[] = "bnhal";
constexpr const char* paths[]{
    "diff_num_b", "diff_num_n", "diff_num_h", "diff_num_a", "diff_num_l",
    "diff_frame_b", "diff_frame_n", "diff_frame_h", "diff_frame_a", "diff_frame_l",
    "msel_assistant1a_bg"
};
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
}

struct AssistantRenderer::State {
    struct Image { alignas(8) std::array<std::byte, 40> args{}; };
    struct Frame {
        std::uintptr_t owner{}, layout{};
        int root{};
        std::array<int, 11> nodes{};
        unsigned drawn{};
        bool ready{}, failed{};
    };
    struct Owned {
        int root{};
        std::array<int, 11> nodes{};
        std::array<std::array<char, 32>, 5> names{};
        unsigned numbers{}, shared{};
        bool moved{};
    };
    MemoryView memory;
    const Catalog& catalog;
    FeatureState& feature;
    CupAfp afp;
    int (__fastcall* image_info)(void*, std::uint32_t, const char*){};
    int (__fastcall* arguments)(void*, const void*){};
    int (__fastcall* bind)(int, const void*, int){};
    int (__fastcall* bitmap)(int, const char*, int){};
    std::array<Image, 246> images{};
    Frame current;
    Owned owned;
    std::uintptr_t slot{};
    std::uint32_t package{};
    DWORD thread{};
    bool attempted{}, prepared{};

    State(const AdapterContext& context, StockBitmap stock)
        : memory(context.memory), catalog(context.catalog), feature(context.state),
          afp(admit_cup_calls(memory, context.modules.afp, context.modules.utils)), bitmap(stock) {
        image_info = reinterpret_cast<decltype(image_info)>(resolve_export(memory, context.modules.utils, 71));
        arguments = reinterpret_cast<decltype(arguments)>(resolve_export(memory, context.modules.utils, 77));
        bind = reinterpret_cast<decltype(bind)>(resolve_export(memory, context.modules.afp, 137));
        require(afp.valid && afp.path_relative && afp.relative && afp.package && afp.property &&
                image_info && arguments && bind && bitmap, "Assistant AFP calls unavailable");
    }
    bool same_thread() const noexcept { return thread && thread == GetCurrentThreadId(); }
    bool live_package() const noexcept {
        int state{};
        std::uint32_t id{};
        return same_thread() && read_field(memory, slot, 0x510, state) && state == 2 &&
            read_field(memory, slot, 0x514, id) && id == package && afp.package(family.data()) == id;
    }
    bool preload(std::uintptr_t asset) {
        slot = asset;
        std::array<char, 256> name{};
        if (!read_field(memory, slot, 8, name) ||
            std::string_view(name.data(), family.size()) != family || name[family.size()] ||
            !read_field(memory, slot, 0x514, package) ||
            (package & 0x78000000U) != 0x30000000U || !live_package()) return false;
        for (unsigned i = 0; i < images.size(); ++i) {
            char resource[40]{};
            if (i < 240) std::snprintf(resource, sizeof(resource), "pz34_as_%c_%03u",
                                      i < 120 ? 'u' : 's', 10 + i % 120);
            else if (i < 245) std::snprintf(resource, sizeof(resource), "pz34_as_frame_%c", letters[i - 240]);
            else std::snprintf(resource, sizeof(resource), "pz34_as_bg");
            alignas(8) std::array<std::byte, 64> info{};
            if (image_info(info.data(), package, resource) || arguments(images[i].args.data(), info.data())) return false;
        }
        return live_package();
    }
    bool preload_checked(std::uintptr_t asset) noexcept {
        __try { return preload(asset); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
    bool resolve(std::uintptr_t owner, Frame& frame) {
        frame = {};
        frame.owner = owner;
        if (!read_field(memory, owner, 8, frame.layout) ||
            !read_field(memory, frame.layout, 0x200, frame.root) ||
            frame.root <= 0 || afp.valid(6, frame.root) < 0) return false;
        for (unsigned i = 0; i < frame.nodes.size(); ++i) {
            const int mc = afp.path_relative(frame.root, paths[i]);
            if (mc <= 0 || afp.valid(6, mc) < 0 || afp.relative(mc, 6) >= 0) return false;
            for (unsigned j = 0; j < i; ++j) if (frame.nodes[j] == mc) return false;
            frame.nodes[i] = mc;
        }
        frame.ready = true;
        return true;
    }
    void begin(std::uintptr_t owner) noexcept {
        __try {
            if (!resolve(owner, current)) return;
            if (owned.root != current.root || owned.nodes != current.nodes) {
                owned = {};
                owned.root = current.root;
                owned.nodes = current.nodes;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) { current.ready = false; }
    }
    bool stock_image(int mc, const char* name) {
        return bitmap(mc, name, 0) >= 0 && afp.property(mc, 0x101E, 1) >= 0;
    }
    bool restore_numbers(unsigned mask) {
        bool ok = true;
        for (unsigned i = 0; i < 5; ++i) if ((owned.numbers & mask) & (1U << i)) {
            if (owned.names[i][0] && stock_image(current.nodes[i], owned.names[i].data())) {
                owned.numbers &= ~(1U << i);
            } else ok = false;
        }
        return ok;
    }
    bool restore_shared() {
        bool ok = true;
        for (unsigned i = 0; i < 6; ++i) if (owned.shared & (1U << i)) {
            char name[32]{};
            if (i < 5) std::snprintf(name, sizeof(name), "ma_diff_frame_%c", letters[i]);
            else std::snprintf(name, sizeof(name), "msel_assistant1_bg");
            if (stock_image(current.nodes[5 + i], name)) {
                owned.shared &= ~(1U << i);
            } else ok = false;
        }
        return ok;
    }
    bool extend_shared() {
        for (unsigned i = 0; i < 6; ++i) if (!(owned.shared & (1U << i))) {
            owned.shared |= 1U << i;
            if (bind(current.nodes[5 + i], images[240 + i].args.data(), 0) ||
                afp.property(current.nodes[5 + i], 0x101E, 1) < 0) return false;
        }
        return true;
    }
    bool position(bool extended) {
        bool ok = true;
        if (extended) owned.moved = true;
        for (unsigned i = 0; i < 5; ++i) {
            const float number[]{
                extended ? decimal_x[i] -
                    ((owned.numbers & (1U << i)) ? origin_shift : 0.f) :
                    native_x[i], centre_y
            };
            const float frame[]{
                extended ? decimal_x[i] - origin_shift :
                    native_x[i], centre_y
            };
            if (afp.property(current.nodes[i], 0x1000, reinterpret_cast<std::intptr_t>(number)) < 0) ok = false;
            if (afp.property(current.nodes[5 + i], 0x1000, reinterpret_cast<std::intptr_t>(frame)) < 0) ok = false;
        }
        if (ok && !extended) owned.moved = false;
        return ok;
    }
    void finish(std::uintptr_t owner) {
        if (!current.ready) return;
        Frame after;
        if (!resolve(owner, after) || after.layout != current.layout || after.root != current.root ||
            after.nodes != current.nodes) return;
        const bool desired = current.drawn && !current.failed && feature.view().enabled() && live_package();
        bool ok = restore_numbers(desired ? ~current.drawn : 31U);
        if (desired && ok) ok = extend_shared() && position(true);
        if (!desired || !ok) {
            restore_numbers(31U);
            restore_shared();
            if (owned.moved) position(false);
        }
    }
    void end(std::uintptr_t owner) noexcept {
        __try { finish(owner); }
        __except (EXCEPTION_EXECUTE_HANDLER) { }
        current = {};
    }
    bool draw(const AssistantHeaderDraw& draw, bool& result) {
        if (!current.ready || draw.owner != current.owner || draw.layout != current.layout ||
            draw.difficulty < 0 || draw.difficulty > 4) return false;
        const auto index = static_cast<unsigned>(draw.difficulty);
        auto& name = owned.names[index];
        std::memcpy(name.data(), draw.texture, name.size());
        const auto row = catalog.find({draw.song, draw.style, draw.difficulty});
        if (!feature.view().enabled() || current.failed || !row || !row->assigned ||
            row->native_level != draw.native_level || row->effective_tenths < 10 ||
            row->effective_tenths > 129 || !live_package()) {
            owned.numbers &= ~(1U << index);
            return false;
        }
        const auto image = static_cast<unsigned>((draw.selected ? 120 : 0) + row->effective_tenths - 10);
        owned.numbers |= 1U << index;
        const int mc = current.nodes[index];
        if (bind(mc, images[image].args.data(), 0) || afp.property(mc, 0x101E, 1) < 0 ||
            !feature.view().enabled()) {
            owned.numbers &= ~(1U << index);
            current.failed = true;
            return false;
        }
        current.drawn |= 1U << index;
        result = true;
        return true;
    }
    bool draw_checked(const AssistantHeaderDraw& request, bool& result) noexcept {
        __try { return draw(request, result); }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            current.failed = true;
            return false;
        }
    }
};

AssistantRenderer::AssistantRenderer(const AdapterContext& context, StockBitmap stock)
    : state_(std::make_unique<State>(context, stock)) {}
AssistantRenderer::~AssistantRenderer() noexcept = default;

bool AssistantRenderer::preload(std::uintptr_t slot) noexcept {
    auto& self = *state_;
    if (self.attempted || self.feature.view().phase != FeaturePhase::preparing) return false;
    self.attempted = true;
    self.thread = GetCurrentThreadId();
    self.prepared = self.preload_checked(slot);
    return self.prepared;
}

AssistantHeaderHook::Callbacks AssistantRenderer::callbacks() noexcept {
    return {
        [](void* opaque, std::uintptr_t owner) noexcept {
            auto& self = *static_cast<AssistantRenderer*>(opaque)->state_;
            if (self.prepared && self.same_thread()) self.begin(owner);
        },
        [](void* opaque, std::uintptr_t owner, bool completed) noexcept {
            auto& self = *static_cast<AssistantRenderer*>(opaque)->state_;
            if (completed) self.end(owner);
            else self.current = {};
        },
        [](void* opaque, const AssistantHeaderDraw& draw, bool& result) noexcept {
            return static_cast<AssistantRenderer*>(opaque)->state_->draw_checked(draw, result);
        }
    };
}
} // namespace point_zero::native
