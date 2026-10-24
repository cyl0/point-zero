#pragma once

#include "memory.hpp"
#include <array>
#include <vector>

namespace point_zero::native {
enum class CategoryLifeEvent {
    queued_initialize, populate, list_destroy, group_destroy, initialize, graph_bind, selection_destroy, scene_entry, scene_tick,
    scene_refresh, preview_assign, selection_restore, style_apply, queued_restore
};
enum class CategoryLifeOutcome { completed, cancelled, unwound, incomplete };
struct CategoryRestoreTarget {
    int category;
    std::uint8_t force_open;
    // READ-ONLY snapshots. Native forwarding ignores writes to these fields.
    int song{-1}, p1{-1}, p2{-1}, extra{-1};
};
struct CategoryLifeRequest {
    CategoryLifeEvent event;
    // Queued callbacks expose their captured scene as owner; raw native RCX
    // is forwarded separately. Other owners/arguments retain their native ABI.
    std::uintptr_t owner{}, argument{};
    std::uintptr_t payload{};
    // Set by the hook only for the admitted ordinary scene/caller route. Other
    // scene calls still enter/leave an observed scope, preventing a nested known
    // completion from being mistaken for the end of an unknown outer borrow.
    bool collection_boundary{};
    // The native style/options refresh, before it captures category/song. Its
    // exact ordinary caller and scene type are required for custom preparation.
    bool refresh_boundary{};
    // Only the exact navigation song-selection caller (game+5FB7B1) permits
    // preview cancellation. Unknown callers still hold an observed scope.
    bool preview_boundary{};
    // Borrowed mutable target only for an exact admitted queued-return or refresh caller.
    // Access is valid ONLY during begin/finish; never retain this pointer.
    // Only category/force_open may be adjusted. The remaining fields are
    // read-only original-argument snapshots; native always receives their
    // original values. Finish sees the target even on cancellation/unwinding;
    // finish changes cannot replay or alter native work already performed.
    CategoryRestoreTarget* restore{};
    // Exact game+8D9091 caller of selection style application (game+5FAB40).
    // Native refresh has already captured its old selection tuple. The owner
    // validates the entered refresh before exchanging roots; no scene read here.
    bool style_boundary{};
    // Exact game+8D90BF caller of selection_restore, later in native refresh.
    // Native owns that restore call; this flag only identifies its route.
    bool refresh_restore{};
    // The queued callback is identified before translating its raw RCX to scene.
    bool initialize_boundary{}, construction_population{};
    std::uintptr_t manager{}, list{}, control{}; // Actual pair around the inlined replacement.
};
struct CategoryLifeSite { std::uintptr_t address{}; std::vector<std::byte> body; };
struct CategoryLifeSites {
    std::array<CategoryLifeSite, 13> entries; // queued_initialize through style_apply.
    CategoryLifeSite queued_restore;
    std::uintptr_t queue_caller{}, initialize_vtable{}, restore_vtable{}, restore_invoke{}, manager_slot{};
    std::uintptr_t restore_caller{}, construction_caller{};
    std::uintptr_t initialize_caller{}; // Exact supported navigation-initializer return address.
    std::uintptr_t scene_entry_caller{}, scene_tick_caller{}, scene_vtable{}, idle_callback{};
    std::uintptr_t scene_refresh_caller{}; // Within the admitted scene_tick body.
    std::uintptr_t preview_caller{}; // Exact game+5FB7B1 return address.
    std::uintptr_t style_apply_caller{}, style_restore_caller{}; // Exact game+8D9091 / game+8D90BF.
};

// Native category boundaries, not a generation registry. The initializer is
// observed only from the admitted selection caller; other callers forward.
// The owner identifies tracked objects and serializes their actual lifetimes.
class CategoryLifecycleHooks final {
public:
    struct Callbacks {
        // Populate and an admitted scene_refresh/style_apply may be cancelled
        // by returning false when preparing safe native vectors failed. An admitted
        // preview_assign may also be cancelled (returning zero); the owner must
        // prove an entered restore targets the exact preview and unchanged song.
        // An admitted selection_restore (restore != nullptr) may also cancel
        // before native work, returning zero with a paired cancelled finish.
        // Other operations, including unknown refresh/style/preview/restore callers, always
        // forward native once. Returning true preserves native RAX.
        bool (*begin)(void*, const CategoryLifeRequest&) noexcept{};
        // Paired even for cancellation/native unwinding. A destructor's owner
        // address is identity only here; the object may already be destroyed.
        // Inner initialize completion records list/control replacement. Publication
        // follows the whole qualified queued_initialize callback with nonzero AL.
        // graph_bind completion may be a guarded return: inspect actual graph+8,
        // not argument. selection_destroy completion is terminal owner identity;
        // never read destroyed selection/graph storage. Neither authorizes bank
        // cleanup before the enclosing native transaction completes.
        // Only completed + collection_boundary permits the owner to collect at
        // its outermost scope. Entry AL=0 reports incomplete, not cancellation
        // or a successful transaction. Tick completion includes its tail calls.
        void (*finish)(void*, const CategoryLifeRequest&, CategoryLifeOutcome) noexcept{};
    };
    CategoryLifecycleHooks(MemoryView, const CategoryLifeSites&, void* context, Callbacks);
    ~CategoryLifecycleHooks() noexcept;
    CategoryLifecycleHooks(const CategoryLifecycleHooks&) = delete;
    CategoryLifecycleHooks& operator=(const CategoryLifecycleHooks&) = delete;
    void arm(); // One attempt; all patch sites checked before enabling.
    bool intact() const noexcept;
    // Drains entered native transactions and their nested lifecycle calls.
    // Never call from a callback or an entered native transaction. After return,
    // new calls forward without reading the observer, context or MemoryView.
    void retire_callbacks() noexcept;
private:
    struct State;
    State* state_{};
};
// Construction/arming/destruction are serialized. Callbacks run on the arming
// thread only and must contain their own faults. Native exceptions propagate.
// Exposed forwarding/code remains resident; no native cleanup at wrapper exit.
} // namespace point_zero::native
