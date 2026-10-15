#pragma once

#include "category_bank.hpp"
#include "category_vector_contract.hpp"

namespace point_zero::native {
enum class CategoryRootsState { empty, prepared, published, rejected, withdrawn, handed_off, uncertain };
struct CategoryRootOwner {
    std::uintptr_t manager{}, list{}, group{};
    bool operator==(const CategoryRootOwner&) const = default;
};
struct CategoryRootSelection {
    std::uintptr_t category{};
    int id{}, style{}, cursor{};
    std::uint8_t open{};
    bool operator==(const CategoryRootSelection&) const = default;
};
// Read the bounded LEVEL linkage. expected additionally rejects a different
// group or an alias under another ID; discovery leaves that policy to its caller.
bool find_level_group(MemoryView, ImageRange, std::uintptr_t list, std::uintptr_t& group,
                      std::uintptr_t expected = 0) noexcept;

// One native LEVEL group's paired root vectors. Owns the unpublished custom
// vectors, then the hidden native originals, never the categories themselves.
// Caller owns the actual list/group/bank lifetime, admitted native boundaries,
// serialized native mutations. No hooks or input polling.
class CategoryRoots final {
public:
    CategoryRoots(MemoryView, ImageRange game, CategoryVectorCalls);
    ~CategoryRoots() noexcept; // No native cleanup; retains any uncertain ownership.
    CategoryRoots(const CategoryRoots&) = delete;
    CategoryRoots& operator=(const CategoryRoots&) = delete;

    // One attempt; bindings is the complete current ready CategoryBank table.
    // Native vector code and category ownership must already be admitted. Build
    // both game-allocated custom vectors before changing the group. Replace exact
    // native LEVEL rows where bank parents exist; preserve every foreign row.
    bool prepare(std::uintptr_t manager, std::uintptr_t list, std::uintptr_t group,
                 std::span<const CategoryAccessBinding> bindings) noexcept;
    bool publish() noexcept; // Revalidate both native roots/cursors, then replace.
    // At a serialized ordinary native LEVEL boundary, after caller closes any
    // native leaf. Requires published roots (no child page), settled style0/1,
    // and a closed current heading plus an exact closed private target parent.
    // Select an existing parent by changing only this style's root cursor;
    // same target succeeds without writing, missing targets never fall back.
    // Caller then enter(level, style) and native607F30 targeting ALL. No native
    // navigation/preview here. Failed cursor stores preserve allocation ownership
    // and leave later navigation available.
    bool select_parent(int level, int style) noexcept;
    // At a closed native LEVEL heading. style is the current native selector;
    // its cursor must select this parent. Prepare ALL/nonempty decimal children
    // for both styles before replacing either root vector. Foreign rows remain
    // visible; the navigation owner leaves before switching to a style without
    // this bank's children.
    // This only changes vectors/cursors: caller performs native navigation/sound.
    bool enter(int level, int style) noexcept;
    // Restore parents, retaining current foreign rows and selected child IDs.
    // Call before external category requests or native population. No-op if the
    // published group already shows roots. Does not refresh/replay song preview.
    bool leave() noexcept;
    int page_level() const noexcept; // Zero when closed/unavailable.
    bool page_available(int style) const noexcept;
    // Dynamic bank IDs; level/part require a populated binding in this style.
    int id(int level, int part) const noexcept;
    int level(int id, int style) const noexcept;
    int part(int id, int style) const noexcept;
    bool owns(std::uintptr_t category, int id, int style) const noexcept;
    CategoryRootOwner owner() const noexcept; // Preparing thread only; not lifetime proof.
    bool current() const noexcept; // Published group still belongs to the current native list.
    // Read-only current LEVEL heading/leaf identity, on the preparing thread.
    // No allocation, native call or protection query. Does not authorize a later
    // mutation; enter/leave still validate their complete ownership separately.
    bool selection(CategoryRootSelection&) const noexcept;
    // At a safe native boundary with no vector iterators retained elsewhere.
    // Replace bank parents with their original native LEVEL rows where present,
    // preserving current foreign membership/order. Closes a child page first.
    bool withdraw() noexcept;
    // Immediately BEFORE the actual native group destructor. Leave its current
    // visible vectors to that destructor; free only our hidden vectors. Caller
    // must still forward native exactly once and wait for completion before
    // retiring categories. handed_off alone does not prove group destruction.
    bool handoff() noexcept;
    // Only unpublished prepare/discard. Never substitutes for group withdrawal.
    bool discard() noexcept;
    CategoryRootsState state() const noexcept;
private:
    struct Backing;
    std::unique_ptr<Backing> backing_;
    bool attempted_{}, busy_{};
    std::uint32_t thread_{};
    bool prepare_checked(std::uintptr_t, std::uintptr_t, std::uintptr_t,
                         std::span<const CategoryAccessBinding>) noexcept;
    bool publish_checked() noexcept;
    bool select_parent_checked(int, int) noexcept;
    bool enter_checked(int, int) noexcept;
    bool leave_checked() noexcept;
    bool withdraw_checked() noexcept;
    bool handoff_checked() noexcept;
    bool discard_checked() noexcept;
    bool invoke(bool (CategoryRoots::*)() noexcept) noexcept;
};
} // namespace point_zero::native
