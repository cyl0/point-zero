#pragma once

#include "adapters.hpp"
#include "played_calls.hpp"
#include "result_plate.hpp"
#include "result_notes.hpp"

namespace point_zero::native {
class PlayedRenderer final {
public:
    static constexpr std::string_view family = "/pz_played.ifs";
    PlayedRenderer(const AdapterContext&, PlayedNativeCalls);
    ~PlayedRenderer() = default; // Never detaches/releases native resources.
    PlayedRenderer(const PlayedRenderer&) = delete;
    PlayedRenderer& operator=(const PlayedRenderer&) = delete;
    PlayedRenderer(PlayedRenderer&&) = delete;
    PlayedRenderer& operator=(PlayedRenderer&&) = delete;
    // Resolve every image and both containers during preparation, before any
    // native publication. Slot is borrowed from the private AssetOwner.
    bool preload(std::uintptr_t slot) noexcept;
    // Admitted native capture boundary: remember result tuples for NOTES and
    // extend an already-placed result plate before the numeral binds.
    void capture(const PlayedCapture&, PlayedStockBitmap) noexcept;
    // Scoped around the native named binder. A nested/unrelated call must also
    // enter/end (empty capture if unknown) so it cannot inherit the outer target.
    void begin(const PlayedCapture&, std::uintptr_t wrapper, const char* name) noexcept;
    void end() noexcept;
    // The shared bitmap hook supplies a bounded name and retained stock call.
    // true means handled, including restoration or uncertain partial mutation;
    // false means forward the native original once. No file I/O or allocation.
    bool bitmap(int mc, const char* name, int mode, int& result, PlayedStockBitmap) noexcept;
    void update_notes(std::uintptr_t wrapper) noexcept;

private:
    struct Image { alignas(8) std::array<std::byte, 40> args{}; };
    struct Owner {
        PlayedChart chart;
        int mc{}, children[3]{}, value{-1};
        played_art::Plan plan;
        bool restoring{}, retired{}, attached{}, children_known{};
    };
    ImageRange game_;
    MemoryView memory_;
    const Catalog& catalog_;
    FeatureState& state_;
    PZ_NativeHost host_;
    PlayedNativeCalls calls_;
    ResultPlate plate_;
    ResultNotes notes_;
    std::array<Image, played_art::image_count> images_{};
    std::array<Owner, 32> owners_{};
    std::uintptr_t slot_{};
    std::uint32_t package_{}, component_{}, thread_{};
    bool attempted_{}, ready_{}, current_valid_{};
    std::uint64_t mutations_{}; // Native writes, including nested calls, for SEH fallback ownership.
    unsigned depth_{};
    PlayedTarget current_;
    bool same_thread() const noexcept;
    bool live() const;
    std::atomic_flag warned_{};
    void fault() noexcept;
    bool preload_impl(std::uintptr_t);
    bool owned(const Owner&) const;
    Owner* find(int) noexcept;
    void collect();
    void begin_impl(const PlayedCapture&, std::uintptr_t, const char*);
    bool bitmap_impl(int, const char*, int, int&, PlayedStockBitmap);
    bool restore(Owner&, int, const char*, int, int&, PlayedStockBitmap);
    bool apply(Owner*, int, const played_art::Plan&, int, const char*, int, int&, PlayedStockBitmap);
};
} // namespace point_zero::native
