#include "build_bindings.hpp"
#include "category_summary_contract.hpp"

#include <stdexcept>
#include <vector>
#include <utility>

namespace point_zero::native {
namespace {
using Pin = build_bindings::CategorySummaryContract::Pin;




// Exact direct targets outside this finite category slice (their call-site
// bindings are retained ABI evidence; transitive engine closure is not admitted):
// 5FDBFC -> 86EF00 metadata resolver: separate admission/routing prerequisite.
// 8705B5/8705ED -> 8937F0 player lamps; 87063B -> 893D30 rival lamps;
// 8705FD/5F9BB0/5F9C07 -> 7FB4B0 player/profile setup;
// 870602/5F9BB5/5F9C0C -> 949480 mode; 87061B -> 949230 player selection;
// 5F9B5B -> 893FA0 candidate policy (separate hook admission),
// 5F9B81 -> 893610 next difficulty; 5F9BA7/5F9BFE -> 893EC0 availability;
// B1BC6C -> B1C600 security failure path. Native exception handlers, including
// 8704B0's B1BB40, are external. No CRT/OS/score/profile implementation rewrite.
// Data dependencies remaining external: security cookie 1094B28, manager
// slots AAB3B08/AAB3B10, music+67Ch and all mutable native score/profile/category
// state. Admission of code and the immutable rival seed literal does not
// validate those values or imply the metadata/candidate hooks are installed.

void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
std::vector<std::byte> copy(MemoryView memory, ImageRange image, Pin pin) {
    const auto size = static_cast<std::size_t>(pin.end - pin.begin);
    require(image.base >= 0x10000 && contains(image.base, image.size, image.base, image.size) &&
            pin.begin <= image.size && size <= image.size - pin.begin,
            "category summary contract extent outside selected image");
    const auto at = image.base + pin.begin;
    require(readable_range(memory, at, size, pin.use, image),
            "category summary contract image ownership/protection changed");
    std::vector<std::byte> bytes(size);
    require(read_bytes(memory, at, bytes), "category summary contract read failed");
    require(readable_range(memory, at, size, pin.use, image),
            "category summary contract image changed during read");
    return bytes;
}
} // namespace

CategorySummarySite admit_category_summary_contract(MemoryView memory, const ImageInfo& game) {
    static_assert(sizeof(std::uintptr_t) == 8);
    // Keep the calculation snapshot for its entry/capture/cancel instruction
    // checks. Other bodies contribute only image extent/protection admission.
    auto body = copy(memory, game.range, game_bindings(game.range).category_summary_contract.pins[0]);
    for (std::size_t i = 1; i < std::size(game_bindings(game.range).category_summary_contract.pins); ++i) {
        const auto pin = game_bindings(game.range).category_summary_contract.pins[i];
        const auto size = static_cast<std::size_t>(pin.end - pin.begin);
        require(pin.begin <= game.range.size && size <= game.range.size - pin.begin &&
                readable_range(memory, game.range.base + pin.begin, size, pin.use, game.range),
                "category summary support extent/protection changed");
    }
    return {game.range.base + game_bindings(game.range).category_summary_contract.category_summary_rva, std::move(body),
            game.range.base + game_bindings(game.range).category_summary_contract.summary_capture_rva, game.range.base + game_bindings(game.range).category_summary_contract.summary_continuation_rva};
}
} // namespace point_zero::native
