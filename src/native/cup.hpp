#pragma once

#include "adapters.hpp"
#include "cup_layout.hpp"
#include "song_select.hpp"

namespace point_zero::native {
// The selected native adapter admits these AFP exports and their used property
// branches before construction. No export lookup, loading or version policy here.
struct CupAfp {
    int (__fastcall* valid)(int, std::uint32_t){};
    int (__fastcall* path)(std::uint32_t, const char*){};
    int (__fastcall* relative)(int, int){};
    std::uint32_t (__fastcall* package)(const char*){};
    int (__fastcall* animation_info)(void*, std::uint32_t, const char*){};
    int (__fastcall* attach)(std::uint32_t, std::uint32_t){};
    int (__fastcall* control)(int, std::uint32_t, ...){};
    int (__fastcall* path_relative)(std::uint32_t, const char*){};
    int (__fastcall* property)(int, std::uint32_t, std::intptr_t){};
};

struct CupNativeCalls {
    int (__fastcall* style)(){};
    std::uintptr_t (__fastcall* clock)(){};
    int (__fastcall* rate)(std::uintptr_t){};
};

class CupRenderer final {
public:
    static constexpr std::string_view family = "/pz34_primary.ifs";
    using StockBitmap = int (__fastcall*)(int, const char*, int);
    CupRenderer(const AdapterContext&, CupAfp, CupNativeCalls);
    ~CupRenderer() = default; // No native release/detach from destruction.
    CupRenderer(const CupRenderer&) = delete;
    CupRenderer& operator=(const CupRenderer&) = delete;
    CupRenderer(CupRenderer&&) = delete;
    CupRenderer& operator=(CupRenderer&&) = delete;
    // Supplied slot belongs to the private AssetOwner. Resolves all241 native
    // components (including dots) before publication, without attaching nodes.
    // False leaves failure/rollback to the initialization owner.
    bool preload(std::uintptr_t slot) noexcept;
    // NativeSurfaces clears before the native setter, then captures its music
    // argument and resulting owner fields after successful return.
    void begin_set() noexcept;
    void bind(std::uintptr_t owner, std::uintptr_t music) noexcept;
    // Called around the original cup update on its native thread. All five
    // values are resolved once; nested updates do not replace the outer context.
    void begin(std::uintptr_t owner) noexcept;
    void end() noexcept;
    // Dispatcher supplies an already bounded, NUL-terminated name and original
    // trampoline. true means handled, including a stock call already performed.
    // Retired/disabled states never attach custom components.
    bool bitmap(int mc, const char* name, int mode, int& result, StockBitmap) noexcept;

private:
    struct Parts {
        std::uint32_t whole[13]{}, fraction[10]{}, point{};
    };
    struct Owner {
        std::uintptr_t cup{};
        std::uint32_t stream{}, component{};
        // Full child handles are valid only for this container/part attachment
        // generation; replacement clears them before discovering new children.
        int mc{}, children[3]{}, inks[3]{}, value{-1}, family{-1};
        std::uint32_t parts[3]{};
        bool restoring{};
    };
    struct Context {
        std::uintptr_t owner{};
        int counter{}, duration{};
        CupValues values;
        bool enabled{};
    };
    MemoryView memory_;
    const Catalog& catalog_;
    FeatureState& state_;
    PZ_NativeHost host_;
    CupAfp afp_;
    CupNativeCalls calls_;
    CupBinding binding_;
    std::array<Parts, 10> bank_{};
    std::array<Owner, 40> owners_{};
    std::uintptr_t slot_{};
    std::uint32_t package_{}, component_{}, thread_{};
    bool attempted_{}, prepared_{};
    unsigned depth_{};
    Context current_;

    bool preload_impl(std::uintptr_t);
    bool live_package() const noexcept;
    bool same_thread() const noexcept;
    void begin_impl(std::uintptr_t);
    bool bitmap_impl(int, const char*, int, int&, StockBitmap);
    Owner* find(int) noexcept;
    void collect();
    std::atomic_flag warned_{};
    void fault() noexcept;
    bool restore(Owner&, int, const char*, int, int&, StockBitmap);
    bool apply(Owner*, int, std::uint32_t, int, int, const char*, int, int&, StockBitmap);
};
} // namespace point_zero::native
