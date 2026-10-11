#pragma once

#include "adapters.hpp"
#include "list_chart.hpp"
#include "list_layout.hpp"
#include "list_hook.hpp"
#include "list_title_hook.hpp"
#include <array>

namespace point_zero::native {
struct ListNativeCalls {
    ListChartCalls chart;
    int (__fastcall* valid)(int, std::uint32_t){};
    std::uint32_t (__fastcall* package)(const char*){};
    int (__fastcall* image_info)(void*, std::uint32_t, const char*){};
    int (__fastcall* arguments)(void*, const void*){};
    int (__fastcall* creation_level)(){};
    int (__fastcall* set_creation_level)(int){};
    std::uintptr_t (__fastcall* emit)(std::uintptr_t, const void*, int, int,
                                    std::uint32_t, std::uint32_t, std::uint32_t){};
    int (__fastcall* visible)(std::uintptr_t, int){};
};
struct ListDraw {
    std::uintptr_t owner{}, return_address{}, manager{};
    const char* name{}; // Original native pointer for badge table identity.
    int x{}, y{};
    std::uint32_t alpha{}, layer{}, flags{}; // Preserve the native stack bits.
};

class ListRenderer final {
public:
    static constexpr std::string_view family = "/pz_list.ifs";
    ListRenderer(const AdapterContext&, ListNativeCalls);
    ~ListRenderer() = default; // Native frame pool/package are never freed here.
    ListRenderer(const ListRenderer&) = delete;
    ListRenderer& operator=(const ListRenderer&) = delete;
    ListRenderer(ListRenderer&&) = delete;
    ListRenderer& operator=(ListRenderer&&) = delete;
    // One attempt on the admitted menu thread while preparing. Resolves230
    // needed glyphs +3 B84 backgrounds using a private AssetOwner's loaded slot.
    bool preload(std::uintptr_t slot) noexcept;
    // true means handled. false means no visible custom submission remains and
    // the hook must forward the native original exactly once. No stock call is
    // made here. On uncertain partial submission, stop custom work and return
    // true instead of duplicating an unverified native draw. Published backing
    // remains retained. OFF/BATTLE simply decline at the single shared gate.
    bool draw(const ListDraw&, std::uintptr_t& result) noexcept;
    // Supply this with context=this to the admitted native ListHook.
    static ListHook::Callback callback() noexcept;
    static ListTitleHook::Permit title_callback() noexcept;
private:
    struct Image { alignas(8) std::array<std::byte, 40> args{}; };
    struct Submitted { std::uintptr_t sprite{}; std::uint32_t stream{}; int root{}; };
    ImageRange game_;
    MemoryView memory_;
    const Catalog& catalog_;
    FeatureState& state_;
    PZ_NativeHost host_;
    ListNativeCalls calls_;
    std::array<std::array<std::array<Image, 25>, 2>, 5> images_{};
    std::array<Image, 3> backgrounds_{};
    std::uintptr_t slot_{};
    std::uint32_t package_{}, thread_{};
    bool attempted_{}, ready_{}, drawing_{};
    std::atomic_flag warned_{};
    void fault() noexcept;
    bool live() const;
    bool preload_impl(std::uintptr_t);
    bool image(Image&, const char*);
    bool draw_impl(const ListDraw&, std::uintptr_t&);
    bool submit(const ListDraw&, const Image* const*, const list_art::Part*, int,
                int before, std::uintptr_t&);
    bool emit_parts(const ListDraw&, const Image* const*, const list_art::Part*, int,
                    int before, std::uintptr_t&);
    bool hide(std::span<const Submitted>, int expected_pool_count);
    bool restore_level(int previous) noexcept;
    bool title_permitted(std::uintptr_t owner) noexcept;
};
// Construction requires already admitted selected call bodies/layouts. Finite
// native contract and scoped draw/title hook wiring are separate responsibilities.
} // namespace point_zero::native
