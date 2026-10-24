#pragma once

#include "memory.hpp"
#include <vector>

namespace point_zero::native {
struct SurfaceHookSite {
    std::uintptr_t address{};
    std::vector<std::byte> body; // Complete expected displaced entry instructions.
};
struct SurfaceHookSites { SurfaceHookSite cup, bitmap, cup_setter, confirmation_setter, confirmation; };

// The selected primary/confirmation setters and updates, and shared AFP bitmap setter. This is not admission
// of the other surfaces, native folders or the complete adapter factory.
class SurfaceHooks final {
public:
    using Cup = std::int64_t (__fastcall*)(std::uintptr_t);
    using CupSetter = std::int64_t (__fastcall*)(std::uintptr_t, std::uintptr_t, int, std::uint8_t);
    using ConfirmationSetter = std::int64_t (__fastcall*)(std::uintptr_t, std::uintptr_t, int, int);
    using Bitmap = int (__fastcall*)(int, const char*, int);
    struct Callbacks {
        void (*begin_cup)(void*, std::uintptr_t) noexcept{};
        void (*end_cup)(void*) noexcept{};
        // name is a bounded local NUL-terminated copy (at most95 characters).
        // true means handled, possibly by calling the supplied original once.
        bool (*bitmap)(void*, int, const char*, int, int&, Bitmap) noexcept{};
        void (*begin_cup_set)(void*) noexcept{};
        void (*bind_cup)(void*, std::uintptr_t, std::uintptr_t) noexcept{};
        void (*begin_confirmation_set)(void*) noexcept{};
        void (*bind_confirmation)(void*, std::uintptr_t, std::uintptr_t, std::uintptr_t) noexcept{};
        void (*begin_confirmation)(void*, std::uintptr_t, std::uintptr_t) noexcept{};
        void (*end_confirmation)(void*) noexcept{};
    };

    // Creates hooks disabled; validates displaced prefixes and owned bridges.
    // No native callback or process publication occurs during construction.
    SurfaceHooks(MemoryView, const SurfaceHookSites&, void* context, Callbacks);
    ~SurfaceHooks() noexcept;
    SurfaceHooks(const SurfaceHooks&) = delete;
    SurfaceHooks& operator=(const SurfaceHooks&) = delete;
    SurfaceHooks(SurfaceHooks&&) = delete;
    SurfaceHooks& operator=(SurfaceHooks&&) = delete;

    // One attempt including rejected preflight. Call on the admitted native
    // menu thread: only that thread invokes callbacks; other threads forward.
    // All originals are callable before any entry is exposed. Until all
    // owned patches pass postcheck, entries only forward stock calls.
    // Failed enable retains backing and permanently consumes the process slot.
    void arm();
    [[nodiscard]] bool intact() const noexcept;
    // The prepared retained original is needed by early result captures before
    // the first bitmap interception. Borrow only while this wrapper is alive;
    // a callback owner must retain us through its drain. This bypasses observers.
    [[nodiscard]] Bitmap stock_bitmap() const noexcept;

    // Close callback admission, drain entered calls, then retire the context.
    // Later native calls still forward through retained code. Does not unhook,
    // free native resources or gate FeatureState. Never call from a callback or
    // an original entered on this hook's admitted thread while its scope is open.
    void retire_callbacks() noexcept;

private:
    struct State;
    State* state_{};
};
// Construction/arm/destruction are externally serialized. Retirement may race
// native calls, including nested bitmap/cup calls. Context must remain alive
// until retirement returns. MemoryView/context must outlive arm/intact.
// Target modules and this core's code remain loaded for process lifetime.
} // namespace point_zero::native
