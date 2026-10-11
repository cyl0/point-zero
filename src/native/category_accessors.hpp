#pragma once

#include "memory.hpp"
#include <array>
#include <vector>

namespace point_zero::native {
// Actual native meanings in entry-address order. Historical Playlister names
// reverse Weekly/KAC; do not infer a summary-byte index from those old names.
enum class CategoryAccessor : unsigned { lookup, active, clear, rival, kac, recommend, shop, weekly, count };
struct CategoryAccessorSite { std::uintptr_t address{}; std::vector<std::byte> body; };
struct CategoryAccessorSites {
    std::array<CategoryAccessorSite, static_cast<unsigned>(CategoryAccessor::count)> entries;
    CategoryAccessorSite style; // Native selector resolver; admitted, never patched.
};
struct CategoryAccessBinding {
    int id{}, style{};
    std::uintptr_t category{}, summary{};
};

// Native accessors for a fixed private namespace, not a bank owner. Unknown IDs
// retain the original game/Playlister route. Definitions, generation construction,
// membership and publication are supplied by the separate native owner.
class CategoryAccessors final {
public:
    // Entries are already admitted complete implementations, including known
    // Playlister chains where present. IDs are sorted/unique in 230..999, <=144.
    CategoryAccessors(MemoryView, const CategoryAccessorSites&, std::span<const int> ids);
    ~CategoryAccessors() noexcept;
    CategoryAccessors(const CategoryAccessors&) = delete;
    CategoryAccessors& operator=(const CategoryAccessors&) = delete;
    void arm(); // One attempt on the native browsing thread; mapping starts empty.
    // Call only at a serialized native bank boundary, before exposing the new
    // generation. Exactly two complete ID sets: style 0 then style 1, each sorted
    // like the constructor span. Both pointers zero means no private category.
    // Nonzero pointers refer to a live 208-byte category and 168-byte summary.
    // Caller proves ownership, matching identities/membership and lifetimes,
    // including ALL delayed native references to any replaced allocations.
    // Validation precedes the copy; failure leaves the existing table unchanged.
    bool bind(std::uintptr_t manager, std::span<const CategoryAccessBinding>) noexcept;
    bool intact() const noexcept;
private:
    struct State;
    State* state_{};
};
// Exposed code and copied pointer table remain process-resident after wrapper
// destruction. This retains pointers, NOT their allocations: the native bank
// owner must preserve them. There are no external callbacks to retire, no table
// clearing at OFF/shutdown, and no per-access allocation/page query. The native
// thread serializes binds/access; foreign-thread private requests return null/0.
} // namespace point_zero::native
