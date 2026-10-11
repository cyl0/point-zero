#include "avs_read.hpp"

#include <stdexcept>
#include <string_view>

namespace point_zero::native {
namespace {
void require(bool okay, const char* message) {
    if (!okay) throw std::runtime_error(message);
}
bool provider(std::string_view text) {
    constexpr std::string_view expected = "avs2-core.dll";
    if (text.size() != expected.size()) return false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char lower = text[i] >= 'A' && text[i] <= 'Z' ? char(text[i] + ('a' - 'A')) : text[i];
        if (lower != expected[i]) return false;
    }
    return true;
}
} // namespace

void check_avs_read_contract(MemoryView memory, const ImageInfo& game, const ImageInfo& avs) {
    // The game imports AVS's read ABI. Spice/IFS own its interception and saved
    // originals; this mod neither calls nor patches their private wrappers.
    const auto imported = inspect_import(memory, game, 0xc91d88);
    const auto exported = inspect_export(memory, avs, 82);
    require(provider(imported.provider) && imported.ordinal == 82 && imported.name.empty() &&
            exported.forwarder.empty() &&
            readable_range(memory, exported.address, 1, RangeUse::image_code, avs.range),
            "AVS read import/provider/export mismatch");
    MemoryRegion target{};
    require(memory.query && memory.query(memory.context, imported.target, target) &&
            contains(target.begin, target.size, imported.target, 1) &&
            target.committed && target.readable && target.executable && !target.guarded,
            "AVS read import target is not executable");
}
} // namespace point_zero::native
