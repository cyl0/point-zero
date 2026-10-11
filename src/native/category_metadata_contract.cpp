#include "category_metadata_contract.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>
#include <utility>

namespace point_zero::native {
namespace {
constexpr std::uint32_t game_rva = 0x86EF00, pl_rva = 0x3FE10;
constexpr std::size_t game_size = 0x3B, pl_size = 0xC2;
constexpr const char* game_sha = "387b888b3cd9055c660209f32531242df6bdb7e30a8f6f47c2b6fe02c5245342";
constexpr const char* pl_sha = "8ca49ba34639dfeb732b945a6770507f05982343669753d6501841adc1108f48";
constexpr std::array<unsigned char, 15> original{
    0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x63,0xDA,0x41,0xB8,0xE6,0,0,0};
constexpr std::array<unsigned char, 6> indirect{0xFF,0x25,0,0,0,0};

void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
template<class T> T value(const std::vector<std::byte>& bytes, std::size_t offset) {
    require(offset <= bytes.size() && sizeof(T) <= bytes.size() - offset,
            "category metadata field outside finite observation");
    T result{};
    std::memcpy(&result, bytes.data() + offset, sizeof(result));
    return result;
}
template<std::size_t N> bool prefix(const std::vector<std::byte>& bytes, std::size_t offset,
                                  const std::array<unsigned char, N>& expected, std::size_t size = N) {
    return size <= N && offset <= bytes.size() && size <= bytes.size() - offset &&
        std::equal(expected.begin(), expected.begin() + size, bytes.begin() + offset,
                   [](unsigned char a, std::byte b) { return a == std::to_integer<unsigned char>(b); });
}
std::uintptr_t address(ImageRange image, std::uint32_t rva, std::size_t size) {
    require(image.base >= 0x10000 && contains(image.base, image.size, image.base, image.size) &&
            rva <= image.size && size <= image.size - rva,
            "category metadata extent outside selected image");
    return image.base + rva;
}
std::vector<std::byte> copy_image(MemoryView memory, ImageRange image, std::uint32_t rva,
                                 std::size_t size, RangeUse use) {
    const auto at = address(image, rva, size);
    require(readable_range(memory, at, size, use, image), "category metadata image ownership/protection changed");
    std::vector<std::byte> bytes(size);
    require(read_bytes(memory, at, bytes), "category metadata image read failed");
    require(readable_range(memory, at, size, use, image), "category metadata image changed during read");
    return bytes;
}
std::uintptr_t relative(std::uintptr_t instruction, std::int32_t displacement) {
    require(instruction <= (std::numeric_limits<std::uintptr_t>::max)() - 5,
            "category metadata relative instruction overflow");
    const auto next = instruction + 5;
    const auto delta = static_cast<std::int64_t>(displacement);
    if (delta < 0) {
        require(next >= static_cast<std::uint64_t>(-delta), "category metadata relative target underflow");
        return next - static_cast<std::uintptr_t>(-delta);
    }
    require(static_cast<std::uint64_t>(delta) <= (std::numeric_limits<std::uintptr_t>::max)() - next,
            "category metadata relative target overflow");
    return next + static_cast<std::uintptr_t>(delta);
}
// Allocation identity, not just MEM_PRIVATE, must survive the copy and both
// observations. SafetyHook suballocations need not start at AllocationBase.
void private_range(MemoryView memory, std::uintptr_t at, std::size_t size, std::uintptr_t allocation) {
    require(readable_range(memory, at, size, RangeUse::private_code),
            "category metadata trampoline is not readable private executable memory");
    const auto end = at + size; // readable_range proved the arithmetic.
    for (auto cursor = at; cursor < end;) {
        MemoryRegion region{};
        require(memory.query && memory.query(memory.context, cursor, region) &&
                contains(region.begin, region.size, cursor, 1) && region.allocation == allocation &&
                region.kind == MemoryKind::private_memory && region.committed && region.readable &&
                region.executable && !region.guarded,
                "category metadata trampoline allocation changed");
        cursor = (std::min)(end, region.begin + region.size);
    }
}
struct Observation {
    CategoryMetadataSite site;
    std::vector<std::byte> game, bindings, saved, size, trampoline;
    std::uintptr_t allocation{};
};
Observation observe(MemoryView memory, const ImageInfo& game, const std::optional<ImageInfo>& pl) {
    Observation result;
    result.game = copy_image(memory, game.range, game_rva, game_size, RangeUse::image_code);
    const auto native = address(game.range, game_rva, game_size);
    if (prefix(result.game, 0, original, 6)) {
        result.site = {native, result.game};
        return result;
    }
    require(pl.has_value(), "category metadata game entry patched without admitted Playlister image");
    const bool e9 = result.game[0] == std::byte{0xE9} && result.game[5] == std::byte{0x90};
    const bool ff = prefix(result.game, 0, indirect) && result.game[14] == std::byte{0x90};
    require(e9 || ff, "category metadata unsupported Playlister game patch shape");
    const auto stolen = e9 ? 6U : 15U;
    const auto trampoline_size = e9 ? 25U : 29U;
    const auto implementation = address(pl->range, pl_rva, pl_size);
    require(game.range.base + game.range.size <= pl->range.base ||
            pl->range.base + pl->range.size <= game.range.base,
            "category metadata game and Playlister images overlap");
    result.site = {implementation, copy_image(memory, pl->range, pl_rva, pl_size, RangeUse::image_code)};
    // Selected stable hook fields only: no mutex, allocator control block,
    // original-byte vector, playlist data, config or external callbacks.
    result.bindings = copy_image(memory, pl->range, 0xFE740, 16, RangeUse::image_read);
    result.saved = copy_image(memory, pl->range, 0xFE760, 16, RangeUse::image_read);
    result.size = copy_image(memory, pl->range, 0xFE788, 8, RangeUse::image_read);
    require(value<std::uintptr_t>(result.bindings, 0) == native &&
            value<std::uintptr_t>(result.bindings, 8) == implementation,
            "category metadata Playlister target/destination binding changed");
    const auto trampoline = value<std::uintptr_t>(result.saved, 0);
    require(value<std::uint64_t>(result.saved, 8) == trampoline_size &&
            value<std::uint64_t>(result.size, 0) == trampoline_size,
            "category metadata Playlister original extent changed");
    require(contains(trampoline, trampoline_size, trampoline, trampoline_size),
            "category metadata Playlister original range overflow");
    // Prove the incoming binding before copying the selected private range.
    require(e9 ? relative(native, value<std::int32_t>(result.game, 1)) == trampoline + 11 :
                 value<std::uintptr_t>(result.game, 6) == implementation,
            "category metadata incoming patch disagrees with Playlister original/destination");
    MemoryRegion region{};
    require(memory.query && memory.query(memory.context, trampoline, region),
            "category metadata Playlister original allocation unavailable");
    result.allocation = region.allocation;
    private_range(memory, trampoline, trampoline_size, result.allocation);
    result.trampoline.resize(trampoline_size);
    require(read_bytes(memory, trampoline, result.trampoline), "category metadata trampoline read failed");
    private_range(memory, trampoline, trampoline_size, result.allocation);
    require(prefix(result.trampoline, 0, original, stolen), "category metadata native stolen bytes changed");
    if (e9) {
        require(result.trampoline[6] == std::byte{0xE9} &&
                relative(trampoline + 6, value<std::int32_t>(result.trampoline, 7)) == native + 6 &&
                prefix(result.trampoline, 11, indirect) &&
                value<std::uintptr_t>(result.trampoline, 17) == implementation,
                "category metadata Playlister E9 relay/original continuation changed");
    } else {
        require(prefix(result.trampoline, 15, indirect) &&
                value<std::uintptr_t>(result.trampoline, 21) == native + 15,
                "category metadata Playlister FF original continuation changed");
    }
    return result;
}
} // namespace

CategoryMetadataSite admit_category_metadata_contract(
    MemoryView memory, const ImageInfo& game, const std::optional<ImageInfo>& playlister) {
    static_assert(sizeof(std::uintptr_t) == 8);
    auto observed = observe(memory, game, playlister);
    return std::move(observed.site);
}
} // namespace point_zero::native
