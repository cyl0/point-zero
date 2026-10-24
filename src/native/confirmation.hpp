#pragma once

#include "cup.hpp"

namespace point_zero::native {
// All calls and absolute addresses are admitted by the selected adapter.
// NativeSurfaces owns the setter/builder hooks and the shared named/bitmap hooks.
struct ConfirmationNativeCalls {
    int (__fastcall* style)(){};
    int (__fastcall* logical_count)(){};
    int (__fastcall* sole_player)(){};
    int (__fastcall* raw_mode)(){};
    std::uintptr_t published_music{}, published_difficulties{}, name_table{};
    std::array<std::uintptr_t, 2> setter_callers{}, named_callers{};
    std::uintptr_t builder_caller{};
};

class ConfirmationRenderer final {
public:
    static constexpr std::string_view family = "/pz34_confirm.ifs";
    using StockBitmap = CupRenderer::StockBitmap;
    ConfirmationRenderer(const AdapterContext&, CupAfp, ConfirmationNativeCalls);
    ~ConfirmationRenderer() = default; // AssetOwner retains published backing.
    ConfirmationRenderer(const ConfirmationRenderer&) = delete;
    ConfirmationRenderer& operator=(const ConfirmationRenderer&) = delete;
    ConfirmationRenderer(ConfirmationRenderer&&) = delete;
    ConfirmationRenderer& operator=(ConfirmationRenderer&&) = delete;

    bool preload(std::uintptr_t slot) noexcept;
    // Clear before the setter; capture only after its successful native return.
    void begin_set() noexcept;
    void bind(std::uintptr_t owner, std::uintptr_t music, std::uintptr_t caller) noexcept;
    // Every entered builder/named call must be balanced, including nested and
    // unrelated calls. caller is the native return address of that wrapper.
    void begin(std::uintptr_t owner, std::uintptr_t caller) noexcept;
    void end() noexcept;
    void begin_named(std::uintptr_t wrapper, const char* name, std::uintptr_t caller) noexcept;
    void end_named() noexcept;
    bool bitmap(int mc, const char* name, int mode, int& result, StockBitmap) noexcept;

private:
    struct Parts { std::uint32_t whole[13]{}, fraction[10]{}, point{}; };
    struct Binding {
        std::uintptr_t owner{}, music{};
        int song{}, style{-1};
        std::array<int, 2> difficulties{}, levels{};
        bool operator==(const Binding&) const = default;
    };
    struct Context {
        Binding binding;
        std::uint32_t stream{};
        int count{};
        bool valid{};
    };
    struct NamedContext {
        const CatalogRow* chart{};
        std::array<int, 8> handles{};
        unsigned count{};
        bool valid{};
    };
    struct Owner {
        std::uintptr_t owner{};
        std::uint32_t stream{}, parts[3]{};
        int mc{}, children[3]{}, inks[3]{}, value{-1}, palette{-1};
        bool restoring{};
    };
    MemoryView memory_;
    const Catalog& catalog_;
    FeatureState& state_;
    PZ_NativeHost host_;
    CupAfp afp_;
    ConfirmationNativeCalls calls_;
    std::array<Parts, 5> bank_{};
    std::array<Owner, 32> owners_{};
    Binding binding_;
    Context current_;
    NamedContext named_;
    std::uintptr_t slot_{};
    std::uint32_t package_{}, component_{}, thread_{};
    unsigned depth_{}, named_depth_{};
    bool attempted_{}, prepared_{};
    std::atomic_flag warned_{};

    bool same_thread() const noexcept;
    bool live_package() const noexcept;
    bool preload_impl(std::uintptr_t);
    bool read_binding(std::uintptr_t, Binding&) const noexcept;
    void begin_impl(std::uintptr_t, std::uintptr_t);
    void begin_named_impl(std::uintptr_t, const char*, std::uintptr_t);
    bool bitmap_impl(int, const char*, int, int&, StockBitmap);
    void collect(Owner&);
    bool restore(Owner&, int, const char*, int, int&, StockBitmap);
    bool apply(Owner*, int, const char*, int, int&, StockBitmap);
    void fault() noexcept;
};
} // namespace point_zero::native
