#pragma once

#include "memory.hpp"
#include <vector>

namespace point_zero::native {
struct CategorySummarySite {
    std::uintptr_t address{};
    std::vector<std::byte> body; // Complete admitted calculation body.
    std::uintptr_t capture{}, cancel{}; // Pointer bind; native cookie/epilogue path.
};
struct CategorySummaryRequest { std::uintptr_t manager{}; int style{}, id{}; };
struct CategorySummaryBinding { std::uintptr_t category{}, summary{}; };

// Keep native calculation, substituting only storage supplied by the owner.
class CategorySummary final {
public:
    struct Callbacks {
        // True supplies retained storage; false forwards the existing calculation.
        bool (*prepare)(void*, const CategorySummaryRequest&, CategorySummaryBinding&) noexcept{};
        // Called once after an accepted preparation, also on binding failure or
        // native exception. completed means the exact bind ran once and native
        // returned; caller still checks storage/identity and handles uncertainty.
        void (*finish)(void*, const CategorySummaryRequest&, const CategorySummaryBinding&, bool completed) noexcept{};
        // Optional observation of native IDs0..229, after their untouched
        // calculation returns or unwinds. No private binding/cancellation;
        // preserve the original return. The entered call holds callback lifetime.
        void (*native_finish)(void*, const CategorySummaryRequest&, std::uintptr_t caller, bool completed) noexcept{};
    };
    CategorySummary(MemoryView, const CategorySummarySite&,
                    void* context, Callbacks);
    ~CategorySummary() noexcept;
    CategorySummary(const CategorySummary&) = delete;
    CategorySummary& operator=(const CategorySummary&) = delete;
    void arm(); // One attempt on the native preparation thread.
    bool intact() const noexcept;
    void retire_callbacks() noexcept; // Never from an entered native calculation.
private:
    struct State;
    State* state_{};
};
// Exposed hooks remain resident; retired callbacks forward to the existing route.
} // namespace point_zero::native
