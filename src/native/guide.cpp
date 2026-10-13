#include "build_bindings.hpp"
#include "guide.hpp"
#include "browsing.hpp"
#include "inline_patch.hpp"
#include "played_mid_patch.hpp"
#include "ratings/ratings.hpp"

#include <windows.h>
#include <intrin.h>
#include <array>
#include <atomic>
#include <cstring>
#include <stdexcept>

namespace point_zero::native {
namespace {
using Pin = build_bindings::Guide::Pin;
constexpr std::array names{"pz_guide8_j", "pz_guide8_a", "pz_guide8_k"};








void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
std::vector<std::byte> code(MemoryView memory, ImageRange game, Pin pin) {
    require(pin.rva <= game.size && pin.size <= game.size - pin.rva,
            "native guide range outside selected game");
    const auto at = game.base + pin.rva;
    std::vector<std::byte> bytes(pin.size);
    require(readable_range(memory, at, bytes.size(), RangeUse::image_code, game) &&
            read_bytes(memory, at, bytes) &&
            readable_range(memory, at, bytes.size(), RangeUse::image_code, game) &&
            (pin.rva != game_bindings(game).guide.capture.rva || ratings::sha256({reinterpret_cast<const char*>(bytes.data()), bytes.size()}) == pin.sha),
            "native guide code changed");
    return bytes;
}

void executable(MemoryView memory, ImageRange game, Pin pin) {
    require(game.base >= 0x10000 && contains(game.base, game.size, game.base, game.size) &&
            pin.rva <= game.size && pin.size <= game.size - pin.rva &&
            readable_range(memory, game.base + pin.rva, pin.size, RangeUse::image_code, game),
            "native guide call range outside executable game image");
}

// This selected game uses release MSVC string/vector layouts. Only a short
// inline string crosses the boundary; every copy, growth and destruction uses
// the game's functions and allocator, not the mod's C++ standard library.
struct String { char data[16]{}; std::size_t size{}, capacity{15}; };
struct Strings { String* begin{}; String* end{}; String* capacity{}; };
static_assert(sizeof(String) == 32 && sizeof(Strings) == 24);

int language(std::uintptr_t guide) noexcept {
    __try {
        const auto package = *reinterpret_cast<const std::uintptr_t*>(guide);
        const auto path = reinterpret_cast<const char*>(package + 8);
        // Use the package the native language/fallback resolver actually chose.
        // No extra language setting or resolver runs on the render hot path.
        for (std::size_t i = 0; i < 64 && path[i]; ++i) {
            if (std::strncmp(path + i, "common_a_10key", 13) == 0) return 1;
            if (std::strncmp(path + i, "common_k_10key", 13) == 0) return 2;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return 0; // The native resolver's default package is Japanese.
}
bool custom_name(const char* name) noexcept {
    if (!name) return false;
    __try {
        for (const auto expected : names)
            if (std::strcmp(name, expected) == 0) return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return false;
}
} // namespace

struct NativeGuide::State {
    using Build = std::uintptr_t (__fastcall*)(std::uintptr_t, Strings*);
    using Copy = std::uintptr_t (__fastcall*)(String*, const String*);
    using Grow = std::uintptr_t (__fastcall*)(Strings*, String*, const String*);
    static std::atomic<State*> live;
    detail::InlinePatch build_patch;
    detail::PlayedMidPatch create_patch;
    const Build original_build;
    const Copy copy;
    const Grow grow;
    const NativeBrowsing& browsing;
    const FolderArtCalls art;
    const std::uintptr_t build_caller, create_call;
    const DWORD thread{GetCurrentThreadId()};
    std::uintptr_t slot{};
    std::atomic<std::uint32_t> users{};
    bool retained{}, armed{};

    State(const AdapterContext& context, const NativeBrowsing& owner, FolderArtCalls calls)
        : build_patch(context.memory, context.modules.game.range.base + game_bindings(context.modules.game.range).guide.builder.rva,
                      code(context.memory, context.modules.game.range, game_bindings(context.modules.game.range).guide.builder),
                      reinterpret_cast<std::uintptr_t>(&build)),
          create_patch(context.memory, context.modules.game.range.base + game_bindings(context.modules.game.range).guide.capture.rva,
                       code(context.memory, context.modules.game.range, game_bindings(context.modules.game.range).guide.capture), create),
          original_build(reinterpret_cast<Build>(build_patch.original())),
          copy(reinterpret_cast<Copy>(context.modules.game.range.base + game_bindings(context.modules.game.range).guide.copy_string.rva)),
          grow(reinterpret_cast<Grow>(context.modules.game.range.base + game_bindings(context.modules.game.range).guide.grow_vector.rva)),
          browsing(owner), art(calls),
          build_caller(context.modules.game.range.base + game_bindings(context.modules.game.range).guide.guide_build_caller_rva),
          create_call(context.modules.game.range.base + game_bindings(context.modules.game.range).guide.guide_create_call_rva) {
        executable(context.memory, context.modules.game.range, game_bindings(context.modules.game.range).guide.factory);
        executable(context.memory, context.modules.game.range, game_bindings(context.modules.game.range).guide.renderer);
        executable(context.memory, context.modules.game.range, game_bindings(context.modules.game.range).guide.copy_string);
        executable(context.memory, context.modules.game.range, game_bindings(context.modules.game.range).guide.grow_vector);
        for (const auto name : names)
            require(art.global_id(name) == -2, "native guide image name already present");
    }
    bool enter() noexcept {
        if (GetCurrentThreadId() != thread) return false;
        auto value = users.load(std::memory_order_relaxed);
        while (!(value & 1U)) {
            if (users.compare_exchange_weak(value, value + 2, std::memory_order_acquire,
                                           std::memory_order_relaxed)) return true;
        }
        return false;
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
    void append(std::uintptr_t guide, Strings* output) {
        if (!browsing.toggle_available()) return;
        String item{};
        const auto name = names[language(guide)];
        item.size = std::strlen(name);
        std::memcpy(item.data, name, item.size + 1);
        if (output->end == output->capacity) grow(output, output->end, &item);
        else {
            copy(output->end, &item);
            ++output->end;
        }
    }
    void append_checked(std::uintptr_t guide, Strings* output) noexcept {
        // Optional decoration failure must not change the shared rating gate.
        try { append(guide, output); } catch (...) {}
    }
    void append_guarded(std::uintptr_t guide, Strings* output) noexcept {
        __try { append_checked(guide, output); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    static std::uintptr_t __fastcall build(std::uintptr_t guide, Strings* output) {
        auto& self = *live.load(std::memory_order_acquire);
        if (reinterpret_cast<std::uintptr_t>(_ReturnAddress()) != self.build_caller || !self.enter())
            return self.original_build(guide, output);
        std::uintptr_t result{};
        __try {
            result = self.original_build(guide, output);
            self.append_guarded(guide, output);
        } __finally { self.leave(); }
        return result;
    }
    static void create(safetyhook::Context& registers) noexcept {
        auto& self = *live.load(std::memory_order_acquire);
        if (custom_name(reinterpret_cast<const char*>(registers.r8))) {
            registers.rdx = self.slot;
            registers.rcx = registers.rax;
            // Replace only MOV RDX,[R15]; MOV RCX,RAX. Resume at the original
            // CALL, so its return address and native unwind coverage are exact.
            registers.rip = self.create_call;
        }
        // Retained immutable package routing also serves an already-built hint
        // after retirement. Stock items execute their original displaced MOVs;
        // no shared factory detour or borrowed owner is needed for this capture.
    }
    bool preload(std::uintptr_t package_slot) noexcept {
        __try {
            const auto package = *reinterpret_cast<const std::uint32_t*>(package_slot + 0x514);
            for (const auto name : names) {
                alignas(8) std::array<std::byte, 56> info{};
                if (art.package_info(info.data(), package, name)) return false;
                std::int32_t id{}, width{}, height{};
                std::memcpy(&id, info.data(), 4);
                std::memcpy(&width, info.data() + 20, 4);
                std::memcpy(&height, info.data() + 24, 4);
                if (id <= 0 || width <= 0 || width > 360 || height != 24) return false;
            }
            slot = package_slot;
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
};
std::atomic<NativeGuide::State*> NativeGuide::State::live{};

NativeGuide::NativeGuide(const AdapterContext& context, const NativeBrowsing& browsing, FolderArtCalls art)
    : state_(new State(context, browsing, art)) {}
NativeGuide::~NativeGuide() noexcept {
    retire_callbacks();
    if (!state_->retained) delete state_;
}
bool NativeGuide::preload(std::uintptr_t slot) noexcept { return state_->preload(slot); }
void NativeGuide::arm() {
    auto& self = *state_;
    require(self.slot && !self.retained && !(self.users.load(std::memory_order_acquire) & 1),
            "native guide unavailable or installation already attempted");
    require(self.build_patch.before_intact() && self.create_patch.before_intact(),
            "native guide changed before installation");
    State* empty{};
    require(State::live.compare_exchange_strong(empty, &self, std::memory_order_acq_rel),
            "native guide installation already present");
    self.retained = true;
    try {
        self.create_patch.enable();
        self.build_patch.enable();
        require(self.build_patch.intact() && self.create_patch.intact(),
                "native guide changed during installation");
        self.armed = true;
    } catch (...) { self.retire(); throw; }
}
bool NativeGuide::rollback() noexcept {
    state_->retire();
    return !state_->retained ||
        (state_->armed && state_->build_patch.intact() && state_->create_patch.intact());
}
void NativeGuide::retire_callbacks() noexcept { if (state_) state_->retire(); }
} // namespace point_zero::native
