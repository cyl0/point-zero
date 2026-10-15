#pragma once

#include "memory.hpp"
#include <array>
#include <vector>

namespace point_zero::native {
enum class CategoryRequestKind { category, song, rival };
enum class CategoryRequestAction { native, reject, heading };
struct CategoryRequest {
    CategoryRequestKind kind;
    std::uintptr_t selection{};
    int category{-1}, song{}, style{-1}, p1{-1}, p2{-1};
};
struct CategoryRequestResult {
    CategoryRequestAction action{CategoryRequestAction::native};
    int category{-1}; // Only song parent -> its ALL child may replace the native candidate.
};
struct CategoryRequestCalls {
    std::uintptr_t (__fastcall* settings)(){}; // 885440.
    std::uint8_t (__fastcall* persistent)(std::uintptr_t manager, int id){}; // 86F990.
    std::uintptr_t (__fastcall* store)(){}; // 883290.
    void (__fastcall* save)(std::uintptr_t store, int id, std::uintptr_t active){}; // 6E7570.
    void (__fastcall* clear)(std::uintptr_t settings, int index){}; // 6E8E60, clear active filter set.
    void (__fastcall* group)(std::uintptr_t list){}; // 63AAF0, reconcile native group cursor.
    std::uintptr_t (__fastcall* sound)(int id){}; // Existing 5C5C80.
};
struct CategoryRequestSite {
    std::uintptr_t address{};
    std::vector<std::byte> body;
    std::uintptr_t capture{}, cancel{}, caller{};
};
struct CategoryRequestSites {
    std::array<CategoryRequestSite, 3> entries; // category609280, song6094A0, rival6098C0.
    std::uintptr_t manager_getter{}; // Admitted no-throw leaf883280, displaced CALL target.
    ImageRange game{};
};

// Three captures AFTER native consumption/eligibility and BEFORE native group
// references. No additional request queue, mailbox consumption or entry hook.
// Caller owns the enclosing
// scene transaction. It must retain banks through the full native continuation
// and drain that enclosing lifecycle before destroying the browsing owner.
class CategoryRequestHooks final {
public:
    using Route = CategoryRequestResult (*)(void*, const CategoryRequest&) noexcept;
    using Failure = void (*)(void*) noexcept;
    // Failure reports an invalid callback result or a failed event/candidate
    // write AFTER route may have changed the page. Ordinary rejection is not a
    // fault. Both callbacks must contain their own faults.
    CategoryRequestHooks(MemoryView, const CategoryRequestSites&, void* context, Route, Failure);
    ~CategoryRequestHooks() noexcept;
    CategoryRequestHooks(const CategoryRequestHooks&) = delete;
    CategoryRequestHooks& operator=(const CategoryRequestHooks&) = delete;
    void arm(); // One attempt on the native browsing thread.
    bool intact() const noexcept;
    // Drain callbacks (including their native helper calls), not the rest of
    // the native consumer. Never retire from an entered callback. Later captures
    // forward without reading borrowed context/MemoryView. Exposed code remains.
    void retire_callbacks() noexcept;
private:
    struct State;
    State* state_{};
};
} // namespace point_zero::native
