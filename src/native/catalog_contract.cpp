#include "catalog_contract.hpp"
#include "ratings/ratings.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace point_zero::native {
namespace {
static_assert(sizeof(std::uintptr_t) == 8 && std::endian::native == std::endian::little);
constexpr std::uintptr_t user_max = 0x00007fffffffffffULL;
constexpr std::uintptr_t header_rva = 0xacd8900, index_rva = 0xacd8910;
constexpr std::uintptr_t read_rva = 0x952c87, import_rva = 0xc91d88, return_rva = 0x952c8d;
constexpr std::size_t stock_capacity = 0x400000, omni_capacity = 0x600000;
struct Pin { std::uintptr_t rva; std::size_t size; };
// Complete retained LDJ2026081900 extents for bounds/permission admission.
// Function bodies are not copied or treated as runtime identity fingerprints.
constexpr std::array pins{
    Pin{0x951e10,47},
    Pin{0x951e40,61},
    Pin{0x951e80,59},
    Pin{0x951fd0,8},
    Pin{0x9522b0,34},
    Pin{0x82dfa0,133},
    Pin{0x3507e0,193},
    Pin{0x33e910,425},
    Pin{0x93ccb0,419},
    Pin{0x940660,684},
    Pin{0x952c20,214},
    Pin{0x952db0,31}
};
void require(bool okay, const char* reason) {
    if (!okay) throw std::runtime_error(reason);
}
bool shaped(std::uintptr_t address, std::size_t length) {
    return address >= 0x10000 && address <= user_max && length && length <= user_max - address;
}
std::uintptr_t add(std::uintptr_t address, std::size_t offset) {
    require(address >= 0x10000 && address <= user_max && offset <= user_max - address,
            "catalogue address overflow or invalid pointer");
    return address + offset;
}
std::uintptr_t relative(std::uintptr_t next, std::int32_t delta) {
    if (delta >= 0) return add(next, static_cast<std::size_t>(delta));
    const auto magnitude = static_cast<std::uintptr_t>(-static_cast<std::int64_t>(delta));
    require(next >= magnitude && shaped(next - magnitude, 1), "catalogue relative pointer underflow");
    return next - magnitude;
}
template<class T> T field(std::span<const std::byte> data, std::size_t offset) {
    require(offset <= data.size() && sizeof(T) <= data.size() - offset, "catalogue field outside copy");
    T value{};
    std::memcpy(&value, data.data() + offset, sizeof(value));
    return value;
}
void hash_is(std::span<const std::byte> data, std::string_view wanted) {
    require(ratings::sha256({reinterpret_cast<const char*>(data.data()), data.size()}) == wanted,
            "catalogue complete code span mismatch");
}
class CatalogMemory {
    MemoryView memory_;
public:
    explicit CatalogMemory(MemoryView memory) : memory_(memory) {
        require(memory.read && memory.query, "catalogue memory callbacks missing");
    }
    void range(std::uintptr_t address, std::size_t size, RangeUse use,
               ImageRange owner = {}, bool storage = false) {
        require(shaped(address, size), "catalogue range overflow or invalid pointer");
        const bool image = use == RangeUse::image_read || use == RangeUse::image_code;
        if (image) require(contains(owner.base, owner.size, address, size), "catalogue range outside image");
        const auto end = address + size;
        std::uintptr_t allocation{};
        std::size_t count{};
        for (auto cursor = address; cursor < end;) {
            MemoryRegion region{};
            require(++count <= 4096 && memory_.query(memory_.context, cursor, region),
                    "catalogue region query failed or fragmented extent");
            require(shaped(region.begin, region.size) && region.allocation >= 0x10000 &&
                    region.allocation <= region.begin && contains(region.begin, region.size, cursor, 1) &&
                    region.committed && region.readable && !region.guarded,
                    "catalogue region invalid or inaccessible");
            if (image) {
                require(region.kind == MemoryKind::image && region.allocation == owner.base,
                        "catalogue image owner mismatch");
            } else {
                if (!allocation) allocation = region.allocation;
                require(region.kind == MemoryKind::private_memory && region.allocation == allocation,
                        "catalogue private allocation mismatch");
            }
            if (use == RangeUse::image_code || use == RangeUse::private_code)
                require(region.executable, "catalogue code is not executable");
            if (storage || use == RangeUse::private_writable)
                require(region.writable && !region.executable, "catalogue storage is not private/data read-write");
            cursor = (std::min)(end, region.begin + region.size);
        }
    }
    std::vector<std::byte> copy(std::uintptr_t address, std::size_t size,
                                RangeUse use, ImageRange owner = {}) {
        range(address, size, use, owner);
        std::vector<std::byte> result(size);
        require(read_bytes(memory_, address, result), "catalogue memory read failed");
        return result;
    }
    std::uintptr_t pointer(std::uintptr_t address, ImageRange owner) {
        return field<std::uintptr_t>(copy(address, sizeof(std::uintptr_t), RangeUse::image_read, owner), 0);
    }

};
void image_shape(ImageRange image) {
    require(shaped(image.base, image.size), "catalogue image range invalid");
}
struct CatalogOmniChain {
    std::uintptr_t relay{}, context{}, trampoline{}, callback{};
};
void check_chain(CatalogMemory& seen, ImageRange game, ImageRange omni,
                 std::span<const std::byte> detour) {
    require(detour.size() == 6 && detour[0] == std::byte{0xe9} && detour[5] == std::byte{0x90},
            "catalogue Omni detour shape mismatch");
    CatalogOmniChain chain;
    chain.relay = relative(add(game.base, read_rva + 5), field<std::int32_t>(detour, 1));
    const auto relay = seen.copy(chain.relay, 14, RangeUse::private_code);
    constexpr std::array indirect{std::byte{0xff},std::byte{0x25},std::byte{0},
                                  std::byte{0},std::byte{0},std::byte{0}};
    require(std::equal(indirect.begin(), indirect.end(), relay.begin()), "catalogue Omni relay mismatch");
    chain.context = field<std::uintptr_t>(relay, 6);
    auto context = seen.copy(chain.context, 0x187, RangeUse::private_code);
    chain.callback = field<std::uintptr_t>(context, 0x177);
    chain.trampoline = field<std::uintptr_t>(context, 0x17f);
    require(chain.callback == add(omni.base, 0x11c0), "catalogue Omni callback owner/address mismatch");
    std::fill(context.begin() + 0x177, context.end(), std::byte{0});
    hash_is(context, "194e3705b88d175140ae903807da6a5955b43c0ced7388172ab7d6e0be06c69c");
    seen.range(chain.callback, 26, RangeUse::image_code, omni);
    const auto trampoline = seen.copy(chain.trampoline, 11, RangeUse::private_code);
    require(trampoline[0] == std::byte{0xff} && trampoline[1] == std::byte{0x15} &&
            trampoline[6] == std::byte{0xe9}, "catalogue Omni trampoline shape mismatch");
    require(relative(add(chain.trampoline, 6), field<std::int32_t>(trampoline, 2)) == add(game.base, import_rva) &&
            relative(add(chain.trampoline, 11), field<std::int32_t>(trampoline, 7)) == add(game.base, return_rva),
            "catalogue Omni relocated read target/continuation mismatch");
}
void check_replacements(CatalogMemory& seen, ImageRange game) {
    // Retain the recognized Omni accessor/load changes without constructing a
    // normalized copy whose bytes have no consumer.
    for (const auto rva : {0x951e28U, 0x951e65U, 0x951e98U, 0x951fd1U}) {
        const auto opcode = seen.copy(add(game.base, rva), 1, RangeUse::image_code, game);
        require(opcode[0] == std::byte{0x8b}, "catalogue Omni accessor replacement mismatch");
    }
    const auto capacity = seen.copy(add(game.base, 0x952c6b), 4, RangeUse::image_code, game);
    const auto branch = seen.copy(add(game.base, 0x952cc4), 2, RangeUse::image_code, game);
    require(field<std::uint32_t>(capacity, 0) == omni_capacity &&
            branch[0] == std::byte{0x90} && branch[1] == std::byte{0x90},
            "catalogue Omni capacity/branch replacement mismatch");
}
} // namespace

CatalogContract check_catalog_contract(MemoryView memory, ImageRange game, ImageRange omnifix) {
    image_shape(game);
    const bool omni = omnifix.base != 0 || omnifix.size != 0;
    if (omni) {
        image_shape(omnifix);
        require(game.base + game.size <= omnifix.base || omnifix.base + omnifix.size <= game.base,
                "catalogue selected images overlap");
    }
    CatalogMemory seen(memory);
    CatalogContract result;
    result.getters = {add(game.base, 0x951fd0)};
    result.load = add(game.base, 0x952c20);
    // Check the slot's image ownership, never assert anything about its target.
    seen.range(add(game.base, import_rva), sizeof(std::uintptr_t), RangeUse::image_read, game);
    for (const auto& pin : pins)
        seen.range(add(game.base, pin.rva), pin.size, RangeUse::image_code, game);
    const auto read_call = seen.copy(add(game.base, read_rva), 6, RangeUse::image_code, game);
    // Prove the chain before accepting the remaining known live replacements.
    if (omni) {
        check_chain(seen, game, omnifix, read_call);
        check_replacements(seen, game);
    } else {
        constexpr std::array expected{std::byte{0xff}, std::byte{0x15}, std::byte{0xfb},
            std::byte{0xf0}, std::byte{0x33}, std::byte{0}};
        require(std::equal(expected.begin(), expected.end(), read_call.begin()),
                "catalogue read call requires a recognized provider route");
    }
    if (omni) {
        const auto header = seen.pointer(add(game.base, header_rva), game);
        const auto index = seen.pointer(add(game.base, index_rva), game);
        const auto buffer = seen.pointer(add(omnifix.base, 0xddaa8), omnifix);
        require(index == add(header, 16) && header == buffer, "catalogue Omni storage pointers disagree");
        seen.range(header, omni_capacity, RangeUse::private_writable, {}, true);
        result.storage = {header, omni_capacity};
    } else {
        result.storage = {add(game.base, header_rva), stock_capacity};
        seen.range(result.storage.address, stock_capacity, RangeUse::image_read, game, true);
    }
    return result;
}
} // namespace point_zero::native
