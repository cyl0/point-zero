#include "build_bindings.hpp"
#include "category_badges_contract.hpp"

#include <stdexcept>

namespace point_zero::native {
namespace {
using Pin = build_bindings::CategoryBadgesContract::Pin;

// Complete bodies, including every chained unwind fragment and all return
// tails. No first-RET/16-byte approximation. The four predicate results are
// AL (uint8_t); false paths can leave the upper return bits unspecified.


// Native producer meaning/order: Weekly byte0, Recommend byte1, Shop byte2,
// KAC byte3. 86E310 calls 8703D0 -> 870240 -> 870320 -> 870170 after population.
// Each visits style, category, visible chart; a match does not stop its scan.
// Those lifecycle bodies are offline ABI evidence, not new admission targets.
// 885430 loads AAB3B10 for Weekly/Recommend, 885400 loads AAB3BA8 for KAC;
// Shop receives the category manager. ChartAt uses chart_count+0Ch and the
// bar vector+20h with one/two reserved rows; MusicOf reads the row's first
// pointer and the producer reads music+67Ch. KAC's third arg is style0/1:
// 888CA0 checks all five event difficulties of that style through 888D90.
// Recommend delegates to 883190 (AAB3B58) and 85C590's three-song membership.
//
// Exact external code dependencies (retained call-site ABI evidence):
// 5CEA10 event/settings owner; 5D59A0 presence; 5C49F0 mode; 9494C0 gate;
// 5AAD20 Weekly retrieval; 5D5690/5D56E0/5D5680 Shop enable/start/end and
// 5D56A0 Shop song retrieval; 5AB860 KAC player gate; 5A2C80 KAC entries.
// The indirect Shop clock call at 86FE4C uses IAT C91F78 (AVS clock); its
// target/provider and downstream OS bodies are NOT admitted here. Shop uses
// seconds in [start,end), so no persistent/per-song predicate cache is valid.
// All mutable event/profile/clock/owner slots, row/music/category memory and
// native exception machinery remain external. Code admission neither proves
// those values nor supplies a fresh population boundary. Stock-only category
// suppression 86F720/86F990 affects IDs110/210..214, not private230..999.
// The retained bodies establish this private-only elision; admission checks bounds;
// there is no extra filter-support callback or custom filter editor.

void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
void check_range(MemoryView memory, ImageRange image, Pin pin) {
    const auto size = static_cast<std::size_t>(pin.end - pin.begin);
    require(image.base >= 0x10000 && contains(image.base, image.size, image.base, image.size) &&
            pin.begin <= image.size && size <= image.size - pin.begin,
            "category badges contract extent outside selected image");
    const auto at = image.base + pin.begin;
    require(readable_range(memory, at, size, RangeUse::image_code, image),
            "category badges contract image ownership/protection changed");
}
} // namespace

CategoryBadgeCalls admit_category_badges_contract(MemoryView memory, const ImageInfo& game) {
    static_assert(sizeof(std::uintptr_t) == 8);
    require(memory.query, "category badges contract memory query missing");
    for (const auto pin : game_bindings(game.range).category_badges_contract.pins) check_range(memory, game.range, pin);
    const auto base = game.range.base;
    return {
        reinterpret_cast<decltype(CategoryBadgeCalls::music_owner)>(base + game_bindings(game.range).category_badges_contract.music_owner_rva),
        reinterpret_cast<decltype(CategoryBadgeCalls::event_owner)>(base + game_bindings(game.range).category_badges_contract.event_owner_rva),
        reinterpret_cast<decltype(CategoryBadgeCalls::chart_at)>(base + game_bindings(game.range).category_badges_contract.chart_at_rva),
        reinterpret_cast<decltype(CategoryBadgeCalls::music_of)>(base + game_bindings(game.range).category_badges_contract.music_of_rva),
        reinterpret_cast<decltype(CategoryBadgeCalls::weekly)>(base + game_bindings(game.range).category_badges_contract.weekly_rva),
        reinterpret_cast<decltype(CategoryBadgeCalls::recommend)>(base + game_bindings(game.range).category_badges_contract.recommend_rva),
        reinterpret_cast<decltype(CategoryBadgeCalls::shop)>(base + game_bindings(game.range).category_badges_contract.shop_rva),
        reinterpret_cast<decltype(CategoryBadgeCalls::kac)>(base + game_bindings(game.range).category_badges_contract.kac_rva),
    };
}
} // namespace point_zero::native
