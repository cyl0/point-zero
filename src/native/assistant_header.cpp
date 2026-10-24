#include "assistant_header.hpp"
#include "call_patch.hpp"

#include <Windows.h>
#include <intrin.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <stdexcept>

using AssistantDispatch = bool (__fastcall*)(std::uintptr_t, const char*, const char*,
                                             std::uintptr_t, std::uintptr_t, std::uintptr_t);
extern "C" {
AssistantDispatch pz_assistant_header_dispatch{};
bool pz_assistant_header_entry(std::uintptr_t, const char*, const char*);
}

namespace point_zero::native {
namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

template<std::size_t N>
bool read_name(MemoryView memory, const char* from, char (&to)[N]) noexcept {
    const auto address = reinterpret_cast<std::uintptr_t>(from);
    for (std::size_t i = 0; i < N; ++i) {
        if (!read_field(memory, address, i, to[i])) return false;
        if (!to[i]) return true;
    }
    return false;
}
}

struct AssistantHeaderHook::State {
    using SetName = bool (__fastcall*)(std::uintptr_t, const char*, const char*);
    using Update = std::uintptr_t (__fastcall*)(std::uintptr_t);
    using Query = int (__fastcall*)();
    enum class Phase { disabled, enabling, armed, failed };
    static std::atomic<bool> reserved;
    static std::atomic<State*> live;
    MemoryView memory;
    detail::CallPatch patch, update;
    SetName original;
    Update original_update;
    Query style, player;
    void* const context;
    const Callbacks callbacks;
    const std::uintptr_t caller, update_caller;
    unsigned depth{};
    std::atomic<Phase> phase{Phase::disabled};
    std::atomic<std::uint32_t> users{};
    DWORD thread{};
    bool retained{};

    State(MemoryView view, const AssistantHeaderSites& sites, void* opaque, Callbacks fn)
        : memory(view),
          patch(view, sites.header, reinterpret_cast<std::uintptr_t>(&pz_assistant_header_entry)),
          update(view, sites.update, reinterpret_cast<std::uintptr_t>(&update_dispatch)),
          original(reinterpret_cast<SetName>(patch.original())),
          original_update(reinterpret_cast<Update>(update.original())),
          style(reinterpret_cast<Query>(sites.style)), player(reinterpret_cast<Query>(sites.player)),
          context(opaque), callbacks(fn), caller(sites.header + 5), update_caller(sites.update + 5) {}

    bool enter(std::uintptr_t return_address, std::uintptr_t expected) noexcept {
        if (return_address != expected || phase.load(std::memory_order_acquire) != Phase::armed ||
            GetCurrentThreadId() != thread) return false;
        auto value = users.load(std::memory_order_relaxed);
        for (;;) {
            if ((value & 1) || value > UINT32_MAX - 2) return false;
            if (users.compare_exchange_weak(value, value + 2, std::memory_order_acquire,
                                           std::memory_order_relaxed)) return true;
        }
    }

    void leave() noexcept {
        if (users.fetch_sub(2, std::memory_order_release) == 3) users.notify_all();
    }

    void retire() noexcept {
        auto value = users.fetch_or(1, std::memory_order_acq_rel) | 1U;
        while (value != 1) {
            users.wait(value, std::memory_order_acquire);
            value = users.load(std::memory_order_acquire);
        }
    }

    bool snapshot(std::uintptr_t layout, const char* path, const char* texture,
                  std::uintptr_t owner, std::uintptr_t difficulty, AssistantHeaderDraw& draw) const noexcept {
        __try {
            if (difficulty >= 5) return false;
            std::uintptr_t current_layout{}, music{};
            std::uint8_t song_valid{}, selected_valid{}, fallback{};
            int selected_chart{};
            std::int8_t level{};
            char observed_path[16]{};
            if (!read_field(memory, owner, 8, current_layout) || current_layout != layout ||
                !read_field(memory, owner, 0x170, music) ||
                !read_field(memory, owner, 0x178, song_valid) || !song_valid ||
                !read_field(memory, owner, 0x180, selected_chart) ||
                !read_field(memory, owner, 0x184, selected_valid) || !selected_valid ||
                !read_field(memory, owner, 0x148, fallback) || fallback ||
                selected_chart < 0 || selected_chart > 9) return false;
            draw.style = style();
            const int physical_player = player();
            if (draw.style < 0 || draw.style > 1 || physical_player < 0 || physical_player > 1 ||
                !read_field(memory, music, 0x67C, draw.song) || draw.song < 1 ||
                !read_field(memory, music, 0x3EC + 5ULL * draw.style + difficulty, level) ||
                level < 1 || level > 12 ||
                !read_name(memory, path, observed_path) || !read_name(memory, texture, draw.texture)) return false;
            draw.owner = owner;
            draw.layout = layout;
            draw.difficulty = static_cast<int>(difficulty);
            draw.native_level = level;
            draw.selected = selected_chart % 5 == draw.difficulty;
            char expected_path[16]{}, expected_texture[32]{};
            std::snprintf(expected_path, sizeof(expected_path), "diff_num_%c", "bnhal"[difficulty]);
            std::snprintf(expected_texture, sizeof(expected_texture),
                          draw.selected ? "ma_diff_num_sel_%02d" : "ma_diff_num_%02d", draw.native_level);
            return std::strcmp(observed_path, expected_path) == 0 &&
                   std::strcmp(draw.texture, expected_texture) == 0;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static bool __fastcall dispatch(std::uintptr_t layout, const char* path, const char* texture,
                                    std::uintptr_t owner, std::uintptr_t difficulty, std::uintptr_t caller) {
        auto& self = *live.load(std::memory_order_acquire);
        if (!self.enter(caller, self.caller)) return self.original(layout, path, texture);
        __try {
            AssistantHeaderDraw draw;
            bool result{};
            if (self.depth == 1 && self.snapshot(layout, path, texture, owner, difficulty, draw) &&
                self.callbacks.draw(self.context, draw, result)) return result;
            return self.original(layout, path, texture);
        } __finally {
            self.leave();
        }
    }

    static std::uintptr_t __fastcall update_dispatch(std::uintptr_t owner) {
        auto& self = *live.load(std::memory_order_acquire);
        if (!self.enter(reinterpret_cast<std::uintptr_t>(_ReturnAddress()), self.update_caller))
            return self.original_update(owner);
        ++self.depth;
        bool completed{};
        __try {
            if (self.depth == 1) self.callbacks.begin(self.context, owner);
            const auto result = self.original_update(owner);
            completed = true;
            return result;
        } __finally {
            if (self.depth == 1) self.callbacks.end(self.context, owner, completed);
            --self.depth;
            self.leave();
        }
    }

};

std::atomic<bool> AssistantHeaderHook::State::reserved{};
std::atomic<AssistantHeaderHook::State*> AssistantHeaderHook::State::live{};

AssistantHeaderHook::AssistantHeaderHook(MemoryView memory, ImageRange image,
                                         const AssistantHeaderSites& sites, void* context, Callbacks callbacks) {
    bool available{};
    require(State::reserved.compare_exchange_strong(available, true),
            "Assistant header hook process slot already reserved");
    try {
        require(memory.read && memory.query && context && callbacks.begin && callbacks.end && callbacks.draw &&
                contains(image.base, image.size, sites.header, 5) &&
                contains(image.base, image.size, sites.update, 5) &&
                contains(image.base, image.size, sites.style, 1) &&
                contains(image.base, image.size, sites.player, 1) &&
                sites.header != sites.update && callable_range(memory, sites.style) &&
                callable_range(memory, sites.player), "Assistant header hook arguments unavailable");
        state_ = new State(memory, sites, context, callbacks);
    } catch (...) {
        State::reserved.store(false, std::memory_order_release);
        throw;
    }
}

AssistantHeaderHook::~AssistantHeaderHook() noexcept {
    state_->retire();
    if (!state_->retained) {
        delete state_;
        State::reserved.store(false, std::memory_order_release);
    }
}

void AssistantHeaderHook::arm() {
    auto& self = *state_;
    require(self.phase.load(std::memory_order_acquire) == State::Phase::disabled,
            "Assistant header hook arm already attempted");
    self.phase.store(State::Phase::failed, std::memory_order_release);
    require(!(self.users.load(std::memory_order_acquire) & 1), "Assistant header callback already retired");
    require(self.patch.before_intact() && self.update.before_intact(), "Assistant header call changed before arm");
    self.thread = GetCurrentThreadId();
    self.retained = true;
    self.phase.store(State::Phase::enabling, std::memory_order_release);
    pz_assistant_header_dispatch = &State::dispatch;
    State::live.store(&self, std::memory_order_release);
    try {
        self.patch.enable();
        self.update.enable();
        require(self.patch.intact() && self.update.intact(), "Assistant header call changed during arm");
        self.phase.store(State::Phase::armed, std::memory_order_release);
    } catch (...) {
        self.retire();
        self.phase.store(State::Phase::failed, std::memory_order_release);
        throw;
    }
}

bool AssistantHeaderHook::intact() const noexcept {
    return state_->phase.load(std::memory_order_acquire) == State::Phase::armed && state_->patch.intact() && state_->update.intact();
}

void AssistantHeaderHook::retire_callbacks() noexcept { state_->retire(); }
}
