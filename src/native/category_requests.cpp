#include "category_requests.hpp"
#include "category_bank.hpp"
#include "played_mid_patch.hpp"

#include <windows.h>
#include <atomic>
#include <bit>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace point_zero::native {
namespace {
void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
bool buffer(std::uintptr_t at, std::size_t size, std::uintptr_t alignment = 8) noexcept {
    return at >= 0x10000 && !(at & (alignment - 1)) && size <= UINTPTR_MAX - at;
}
bool intersects(const CategoryRequestSite& s, std::uintptr_t at, std::size_t size) noexcept {
    return size && size <= UINTPTR_MAX - at && at < s.address + s.body.size() && at + size > s.address;
}
int integer(std::uintptr_t value) noexcept {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}
bool difficulty(int value) noexcept { return value >= -1 && value <= 4; }
bool parent(int id) noexcept { return CategoryBank::part(id) == 0; }
bool writable(MemoryView m, std::uintptr_t at) noexcept {
    MemoryRegion r{};
    return m.query && m.query(m.context, at, r) && contains(r.begin, r.size, at, sizeof(int)) &&
        r.committed && r.readable && r.writable && !r.guarded && !r.executable;
}
bool replace(MemoryView m, std::uintptr_t at, int old, int next) noexcept {
    // The route may call native helpers. Recheck both readers and the actual
    // slot after it returns, then make exactly one conditional four-byte write.
    int value{};
    return read_value(m, at, value) && value == old && writable(m, at) &&
        (m == current_process_memory() || writable(current_process_memory(), at)) &&
        InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(at), next, old) == old;
}
using category_layout::Vector;
struct Capture {
    CategoryRequest request{};
    std::uintptr_t event{}, vector{};
    int event_value{};
    Vector candidates{};
};
} // namespace

struct CategoryRequestHooks::State {
    enum class Phase { disabled, enabling, armed, failed };
    static std::atomic<bool> reserved;
    static std::atomic<State*> live;
    static constinit thread_local bool routing;
    const MemoryView memory;
    const CategoryRequestSites sites;
    std::array<std::unique_ptr<detail::PlayedMidPatch>, 3> patches;
    void* const context;
    const Route route;
    const Failure failure;
    std::atomic<Phase> phase{Phase::disabled};
    std::atomic<std::uint32_t> users{}; // Closed bit; callback leases in increments of two.
    DWORD thread{};
    bool retained{};

    State(MemoryView m, const CategoryRequestSites& s, void* c, Route r, Failure f)
        : memory(m), sites(s), context(c), route(r), failure(f) {
        require(read_getter(), "category request manager getter is not the admitted eight-byte leaf");
        constexpr std::array<safetyhook::MidHookFn, 3> callbacks{category, song, rival};
        for (std::size_t i = 0; i < patches.size(); ++i) {
            const auto& site = sites.entries[i];
            patches[i] = std::make_unique<detail::PlayedMidPatch>(memory, site.capture,
                std::span(site.body).subspan(site.capture - site.address, 5), callbacks[i]);
        }
        require(before_intact(), "category requests changed during construction");
    }
    bool read_getter() noexcept {
        __try {
            std::array<std::byte, 8> getter{}, actual{};
            return read_bytes(memory, sites.manager_getter, getter) &&
                (memory == current_process_memory() ||
                 (read_bytes(current_process_memory(), sites.manager_getter, actual) && getter == actual)) &&
                getter[0] == std::byte{0x48} && getter[1] == std::byte{0x8b} &&
                getter[2] == std::byte{0x05} && getter[7] == std::byte{0xc3};
        } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
    bool before_intact() const noexcept {
        __try {
            for (const auto& p : patches) if (!p->before_intact()) return false;
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
    bool patches_intact() const noexcept {
        __try {
            for (const auto& p : patches) if (!p->intact()) return false;
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
    bool intact() const noexcept {
        return phase.load(std::memory_order_acquire) == Phase::armed && patches_intact();
    }
    bool enter() noexcept {
        if (routing || phase.load(std::memory_order_acquire) != Phase::armed || GetCurrentThreadId() != thread)
            return false;
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
        while (value != 1) { users.wait(value, std::memory_order_acquire); value = users.load(std::memory_order_acquire); }
    }
    bool frame_is(std::size_t index, const safetyhook::Context& r) const noexcept {
        constexpr std::array<std::size_t, 3> frames{0x98, 0x108, 0xd8};
        const auto frame = frames[index];
        if (!buffer(r.rsp, frame + 0x28, 16)) return false;
        // Reject fabricated frames before consulting the borrowed reader. The
        // native return slot and song/rival locals must belong to this stack.
        const auto* tib = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
        const auto low = reinterpret_cast<std::uintptr_t>(tib->StackLimit);
        const auto high = reinterpret_cast<std::uintptr_t>(tib->StackBase);
        if (high < low || !contains(low, high - low, r.rsp, frame + 0x28)) return false;
        const auto entry = r.rsp + frame;
        std::uintptr_t caller{};
        if (!read_value(current_process_memory(), entry, caller) || caller != sites.entries[index].caller)
            return false;
        if (index == 1 && (r.rbp != r.rsp + 0xa9 || r.r14 != r.rsp + 0x88)) return false;
        if (index == 2 && r.rsi != r.rsp + 0x88) return false;
        return true;
    }
    bool decode(std::size_t index, const safetyhook::Context& r, Capture& out) const noexcept {
        constexpr std::array<std::size_t, 3> frames{0x98, 0x108, 0xd8};
        const auto entry = r.rsp + frames[index];
        std::uintptr_t caller{};
        if (!read_value(memory, entry, caller) || caller != sites.entries[index].caller) return false;
        auto& request = out.request;
        request.kind = static_cast<CategoryRequestKind>(index);
        if (index == 0) {
            request.selection = r.r15;
            out.event = r.r14;
            request.category = integer(r.rbp);
        } else if (index == 1) {
            if (!read_value(memory, entry + 8, request.selection) ||
                !buffer(request.selection, 0x330) || r.r13 != request.selection + 0x328 ||
                !read_value(memory, entry + 0x10, out.event)) return false;
            std::array<int, 3> record{};
            out.vector = r.rbp - 0x69;
            if (!read_value(memory, r.r14, record) || !difficulty(record[0]) ||
                !read_value(memory, r.rbp + 0x77, request.p2) ||
                !read_value(memory, out.vector, out.candidates)) return false;
            const auto& v = out.candidates;
            // 606640 copies exactly [RBP+7F,RBP+87): two ints, capacity two.
            if (!buffer(v.begin, 8, 4) || v.end != v.begin + 8 || v.capacity != v.end ||
                (r.rsi != v.begin && r.rsi != v.begin + 4) ||
                !read_value(memory, r.rsi, request.category)) return false;
            request.song = record[2]; request.style = record[1]; request.p1 = integer(r.r12);
        } else {
            request.selection = r.r14; out.event = r.r15; request.category = 197;
            std::array<int, 5> record{};
            if (!read_value(memory, r.rsi, record)) return false;
            request.song = record[4]; request.style = record[3]; request.p1 = request.p2 = record[2];
        }
        if (!buffer(request.selection, 0x330) || !buffer(out.event, sizeof(int), 4) ||
            request.category < 0 || !read_value(memory, out.event, out.event_value)) return false;
        std::byte selection{};
        if (!read_value(memory, request.selection, selection)) return false;
        if (!index) return out.event_value != 9;
        return request.song > 0 && (request.style == 0 || request.style == 1) &&
            difficulty(request.p1) && difficulty(request.p2);
    }
    void dispatch(std::size_t index, safetyhook::Context& registers) noexcept {
        if (!enter()) return; // No borrowed MemoryView/context access after closure.
        routing = true;
        bool matched{}, called{}, failed{};
        __try {
            __try {
                if (!frame_is(index, registers)) return;
                matched = true;
                Capture capture{};
                if (!decode(index, registers, capture)) {
                    registers.rip = sites.entries[index].cancel;
                    return;
                }
                called = true;
                const auto result = route(context, capture.request);
                const auto old = capture.request.category;
                const bool unchanged = result.category == -1 || result.category == old;
                if (result.action == CategoryRequestAction::native) {
                    if (unchanged) return;
                    Vector current{};
                    if (index == 1 && parent(old) && result.category == old + 1 &&
                        read_value(memory, capture.vector, current) && current == capture.candidates &&
                        replace(memory, registers.rsi, old, result.category)) return;
                } else if (result.action == CategoryRequestAction::heading && index == 0 && unchanged) {
                    if (replace(memory, capture.event, capture.event_value, 3)) {
                        registers.rip = sites.entries[index].cancel;
                        return;
                    }
                } else if (result.action == CategoryRequestAction::reject) {
                    registers.rip = sites.entries[index].cancel;
                    return;
                }
                failed = true;
                // Reject, heading, or an invalid callback result. Preserve RAX
                // and every other native register. Song must run next/free.
                registers.rip = sites.entries[index].cancel;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                // Once the real caller/frame is established, failed decoding
                // must not run the manager/group continuation. Only an entered
                // route may have committed owner state and needs notification.
                failed = called;
                if (matched) registers.rip = sites.entries[index].cancel;
            }
            // Separate boundary: even a fault in Failure is contained and must
            // not cause a second notification. The callback lease covers it.
            if (failed) {
                __try { failure(context); }
                __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
        } __finally { routing = false; leave(); }
    }
    static void capture(std::size_t index, safetyhook::Context& r) noexcept {
        if (auto* self = live.load(std::memory_order_acquire)) self->dispatch(index, r);
    }
    static void category(safetyhook::Context& r) noexcept { capture(0, r); }
    static void song(safetyhook::Context& r) noexcept { capture(1, r); }
    static void rival(safetyhook::Context& r) noexcept { capture(2, r); }
};
std::atomic<bool> CategoryRequestHooks::State::reserved{};
std::atomic<CategoryRequestHooks::State*> CategoryRequestHooks::State::live{};
constinit thread_local bool CategoryRequestHooks::State::routing{};

CategoryRequestHooks::CategoryRequestHooks(MemoryView memory, const CategoryRequestSites& sites, void* context, Route route, Failure failure) {
    bool available{};
    require(State::reserved.compare_exchange_strong(available, true), "category request process slot already reserved");
    try {
        require(memory.read && memory.query && context && route && failure && sites.manager_getter >= 0x10000 &&
            sites.manager_getter <= INT64_MAX - 8, "category request dependencies invalid");
        for (std::size_t i = 0; i < sites.entries.size(); ++i) {
            const auto& s = sites.entries[i];
            require(s.address >= 0x10000 && s.address <= INT64_MAX && !s.body.empty() &&
                s.body.size() <= static_cast<std::uintptr_t>(INT64_MAX) - s.address &&
                contains(s.address, s.body.size(), s.capture, 5) && s.capture > s.address &&
                contains(s.address, s.body.size(), s.cancel, 1) && s.cancel >= s.capture + 5 &&
                s.caller >= 0x10000 && s.body[s.capture - s.address] == std::byte{0xe8},
                "category request complete body/capture/cancel/caller invalid");
            std::int32_t delta{};
            std::memcpy(&delta, s.body.data() + (s.capture - s.address) + 1, sizeof(delta));
            require(static_cast<std::int64_t>(sites.manager_getter) - static_cast<std::int64_t>(s.capture + 5) == delta,
                "category request capture is not the admitted manager getter CALL");
            require(!intersects(s, sites.manager_getter, 8), "category request body overlaps getter");
            for (std::size_t j = 0; j < i; ++j)
                require(!intersects(s, sites.entries[j].address, sites.entries[j].body.size()), "category request bodies overlap");
        }
        state_ = new State(memory, sites, context, route, failure);
    } catch (...) { State::reserved.store(false, std::memory_order_release); throw; }
}
CategoryRequestHooks::~CategoryRequestHooks() noexcept {
    state_->retire();
    if (!state_->retained) { delete state_; State::reserved.store(false, std::memory_order_release); }
}
void CategoryRequestHooks::arm() {
    auto& self = *state_;
    auto expected = State::Phase::disabled;
    require(self.phase.compare_exchange_strong(expected, State::Phase::failed, std::memory_order_acq_rel),
        "category request arm already attempted");
    require(!(self.users.load(std::memory_order_acquire) & 1) && self.before_intact(), "category requests retired/changed before arm");
    self.thread = GetCurrentThreadId();
    self.retained = true; // Includes the first failing enable attempt.
    self.phase.store(State::Phase::enabling, std::memory_order_release);
    State::live.store(&self, std::memory_order_release);
    try {
        for (auto& patch : self.patches) patch->enable();
        require(self.patches_intact(), "category request patch ownership changed during arm");
        self.phase.store(State::Phase::armed, std::memory_order_release);
    } catch (...) {
        self.retire();
        self.phase.store(State::Phase::failed, std::memory_order_release);
        throw;
    }
}
bool CategoryRequestHooks::intact() const noexcept { return state_->intact(); }
void CategoryRequestHooks::retire_callbacks() noexcept { state_->retire(); }
} // namespace point_zero::native
