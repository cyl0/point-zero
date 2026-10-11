#pragma once

#include "category_accessors.hpp"

namespace point_zero::native {
struct CategoryBadgeCalls {
    std::uintptr_t (__fastcall* music_owner)(){};
    std::uintptr_t (__fastcall* event_owner)(){};
    std::uintptr_t (__fastcall* chart_at)(std::uintptr_t category, int index){};
    std::uintptr_t (__fastcall* music_of)(std::uintptr_t chart){};
    std::uint8_t (__fastcall* weekly)(std::uintptr_t owner, unsigned song){};
    std::uint8_t (__fastcall* recommend)(std::uintptr_t owner, unsigned song){};
    std::uint8_t (__fastcall* shop)(std::uintptr_t manager, unsigned song){};
    std::uint8_t (__fastcall* kac)(std::uintptr_t owner, unsigned song, unsigned style){};
};
enum class CategoryBadgeResult { rejected, complete, uncertain };

// A synchronous fresh-bank operation, after final visible membership is ready
// and before publication. Caller owns and serializes manager, entries and ALL
// category/summary/chart/bar allocations, admits the calls and retains their
// code. Entries are unique, ordered by style then private ID (230..999), at most
// 144 per style. Both pointers zero represents an absent category. This is not
// an eligibility test or a score calculation, and installs no hooks.
//
// Preserve native producer order: badge, style/category, visible chart. Call
// every native predicate, including repeats and rows after a positive result;
// Shop reads the native clock. No per-song cache or per-frame refresh. The four
// bytes are Weekly/Recommend/Shop/KAC, irrespective of legacy getter names.
//
// Capture visible membership before native evaluation. Publish only bytes0..3;
// native chart/music bindings are checked per provider, with failed providers
// skipped independently. No summary snapshot or repeated membership pass.
// Rejected means no
// owned summary write was attempted; uncertain means a write was attempted
// without a verified complete result. Neither outcome proves native withdrawal
// or authorizes freeing a published bank. SEH/C++ faults are contained without
// replay. Underlying native event/clock values are not an atomic snapshot.
CategoryBadgeResult refresh_category_badges(MemoryView, const CategoryBadgeCalls&,
    std::uintptr_t manager, std::span<const CategoryAccessBinding>) noexcept;
} // namespace point_zero::native
