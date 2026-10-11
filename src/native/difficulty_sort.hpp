#pragma once

#include "adapters.hpp"
#include "category_layout.hpp"

namespace point_zero::native {
using SortCompare = int (__cdecl*)(const void*, const void*);
using NativeQSort = void (__cdecl*)(void*, std::size_t, std::size_t, SortCompare);
struct SortNativeCalls {
    int (__fastcall* mode)(){};
    SortCompare level{};
};
struct SortRequest {
    std::uintptr_t category{}, caller{}, bars{};
    std::size_t count{}, width{};
    SortCompare compare{};
    std::uint8_t visible{}; // Captured native R15B, not inferred from bar contents.
};
struct SortOwnedCategory {
    void* context{};
    // The final browsing owner verifies only its own retained categories.
    // Called again before/after publication. Never infer ownership from an ID.
    bool (*verify)(void*, std::uintptr_t category, int id, int style,
                   std::uintptr_t list) noexcept{};
};

// Visible difficulty-sort operation only. Native eligibility/backing rows,
// category publication and toggle/filter transactions are owned by browsing.
// Calls and the qsort continuation must already have their code admitted.
class DifficultySort final {
public:
    DifficultySort(const AdapterContext&, SortNativeCalls, SortOwnedCategory);
    DifficultySort(const DifficultySort&) = delete;
    DifficultySort& operator=(const DifficultySort&) = delete;
    // One preparation attempt, on the future sorting thread, before publication.
    // Reserve all scratch storage now; sort never allocates or performs file I/O.
    void prepare();
    // True means handled (including uncertainty after attempting a native write).
    // False leaves the original qsort call to the hook exactly once. The native
    // original is used on PRIVATE bars first to capture its actual tie order.
    // effective is the published policy during normal browsing, or the target
    // of the caller's serialized refresh. Do not publish it before that whole
    // operation completes. Native ownership/mode checks still apply.
    bool sort(const SortRequest&, NativeQSort original, bool effective) noexcept;

private:
    static constexpr std::size_t limit = 20000;
    struct Entry {
        category_layout::Bar bar;
        category_layout::Chart chart;
        int song{}, key{};
        std::size_t rank{};
    };
    struct Context {
        std::uintptr_t manager{}, list{};
        int style{}, mode{}, dp{}, players{};
        std::array<int, 2> active{};
        std::uint8_t bars{};
        bool operator==(const Context&) const = default;
    };
    AdapterContext context_;
    SortNativeCalls calls_;
    SortOwnedCategory owned_;
    std::vector<Entry> entries_;
    std::vector<std::uint32_t> chart_entries_;
    std::vector<category_layout::Bar> before_, work_;
    std::uint32_t thread_{};
    bool attempted_{}, prepared_{}, busy_{}, write_attempted_{};
    bool observe(Context&) const;
    bool owns(const SortRequest&, const category_layout::Category&, const Context&) const;
    bool run(const SortRequest&, NativeQSort);
    std::atomic_flag warned_{};
    void fault() noexcept;
};
} // namespace point_zero::native
