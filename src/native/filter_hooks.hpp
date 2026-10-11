#pragma once

#include "memory.hpp"
#include <vector>

namespace point_zero::native {
using NativeFilterApply = bool (__fastcall*)(std::uintptr_t owner, int set, std::uintptr_t candidate);
using NativeFilterRebuild = void (__fastcall*)(std::uintptr_t category, std::uint8_t filtered, int style);
struct FilterApplyRequest {
    std::uintptr_t owner{}, candidate{}, caller{};
    int set{}, category{}, style{};
};
struct FilterSite { std::uintptr_t address{}; std::vector<std::byte> body; };
struct FilterSites {
    FilterSite apply, level, candidate; // candidate is admitted but not patched.
    std::uintptr_t apply_caller{}, level_caller{};
};
class FilterHooks final {
public:
    struct Callbacks {
        // Execute the original application once. Native exceptions may propagate;
        // the hook drains/restores its scope even when they do.
        bool (*apply)(void*, const FilterApplyRequest&, NativeFilterApply){};
        // Only inside an entered set1 application at its exact predicate caller.
        // True supplies reject; false forwards the original leaf once.
        bool (*level)(void*, std::uintptr_t filter, std::uintptr_t candidate, bool& reject) noexcept{};
    };
    FilterHooks(MemoryView, const FilterSites&, void* context, Callbacks);
    ~FilterHooks() noexcept;
    FilterHooks(const FilterHooks&) = delete;
    FilterHooks& operator=(const FilterHooks&) = delete;
    void arm(); // One attempt, disabled until both patch sites are checked.
    bool intact() const noexcept;
    void retire_callbacks() noexcept; // Never from an entered application scope.
private:
    struct State;
    State* state_{};
};
// Exposed forwarding remains retained for process lifetime. Every application,
// even an unknown one, shadows any outer predicate scope. Retirement blocks new
// applications but permits an entered one to finish its predicate callbacks.
} // namespace point_zero::native
