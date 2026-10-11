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

// Keep native lamp/rival calculation. Redirect only a registered private ID's
// category and 168-byte summary storage, before native category dereferences.
// This does not own the bank, publish it, interpret scores or duplicate the
// native algorithm. Metadata routing and complete call admission are required.
class CategorySummary final {
public:
    struct Callbacks {
        // Read-only owner check. True supplies live private storage for this
        // exact manager/style/ID. Caller validates category/membership/metadata
        // and retains both allocations through finish. False refuses the call;
        // a private ID must never fall through to native indexed storage.
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
    // Sorted unique private IDs in230..999, at most144; copied before arming.
    CategorySummary(MemoryView, const CategorySummarySite&, std::span<const int> ids,
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
// Construction/arm/destruction are serialized. Exposed hooks/forwarding stay
// resident. Unknown IDs always forward once, shadowing any outer private scope.
// Wrong-thread/retired/refused private calls return zero without native effects.
// The hook itself performs no per-call page query, allocation, native scan,
// summary cache or retry. Bank checks remain the owner's responsibility.
} // namespace point_zero::native
