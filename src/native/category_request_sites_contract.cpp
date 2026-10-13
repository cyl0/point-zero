#include "build_bindings.hpp"
#include "category_request_sites_contract.hpp"
#include "ratings/ratings.hpp"

#include <windows.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <utility>

namespace point_zero::native {
namespace {
using Pin = build_bindings::CategoryRequestSitesContract::Pin;

// Retained LDJ2026081900 game SHA c61b6dcb8894062e56d60da8ca90053b27f129e1a8e8da5e54457aa42602397d.
// Complete normal bodies, not short site signatures. Consumer/caller EH funclets
// and transitive engine calls remain outside this finite admission.




// Retained Fervidex SHA0d3f2f10ff140e96cf5b473257ed34b6feb77828646583398f13a16ff5f47648.
// The request leaf's tail wrapper and all five helper unwind fragments are
// included. No selected body has a DIR64 relocation. Other code is not vetoed.
constexpr std::array<Pin, 3> foreign_pins{{
    {0xD77C0, 0xD789E},
    {0xD7A90, 0xD7A9D},
    {0xD7CD0, 0xD7F03},
}};


constexpr std::array<std::array<unsigned char, 5>, 2> originals{{
    {0xE8, 0x73, 0x47, 0xF1, 0xFF}, {0xE8, 0xE5, 0xC7, 0x1C, 0x00}
}};
constexpr std::array<unsigned char, 6> indirect{0xFF, 0x25, 0, 0, 0, 0};
// Fixed template at retained Fervidex+1DA040:375 instruction bytes followed by
// zero callback/trampoline slots. Same reviewed Windows x64 SafetyHook template
// as our captures; accept no code byte learned from the observed allocation.
constexpr const char* stub_sha = "b99e270acb96c810c539d96db7f03fdf4e68e23a4938253582e4d8d86e07d357";

void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
std::uintptr_t address(ImageRange image, std::uint32_t rva, std::size_t size) {
    require(image.base >= 0x10000 && contains(image.base, image.size, image.base, image.size) &&
            rva <= image.size && size <= image.size - rva,
            "request sites extent outside selected image");
    return image.base + rva;
}
template<class T> T field(std::span<const std::byte> bytes, std::size_t offset) {
    require(offset <= bytes.size() && sizeof(T) <= bytes.size() - offset, "request sites field outside copy");
    T value{}; std::memcpy(&value, bytes.data() + offset, sizeof(value)); return value;
}
bool prefix(std::span<const std::byte> bytes, std::span<const unsigned char> expected) {
    return bytes.size() >= expected.size() && std::equal(expected.begin(), expected.end(), bytes.begin(),
        [](unsigned char a, std::byte b) { return a == std::to_integer<unsigned char>(b); });
}
std::uintptr_t relative(std::uintptr_t instruction, std::int32_t delta) {
    require(instruction >= 0x10000 && instruction <= INT64_MAX - 5, "request relative instruction invalid");
    const auto after = static_cast<std::int64_t>(instruction) + 5;
    require(delta <= 0 || after <= INT64_MAX - delta, "request relative target overflow");
    const auto target = after + delta;
    require(target >= 0x10000, "request relative target invalid");
    return static_cast<std::uintptr_t>(target);
}
std::vector<std::byte> copy(MemoryView memory, ImageRange image, std::uint32_t rva, std::size_t size) {
    const auto at = address(image, rva, size);
    require(readable_range(memory, at, size, RangeUse::image_code, image), "request image code ownership changed");
    std::vector<std::byte> bytes(size);
    require(read_bytes(memory, at, bytes), "request image code unavailable");
    require(readable_range(memory, at, size, RangeUse::image_code, image), "request image code changed during copy");
    return bytes;
}
void hash(std::span<const std::byte> bytes, const char* expected) {
    require(ratings::sha256({reinterpret_cast<const char*>(bytes.data()), bytes.size()}) == expected,
            "private relocation stub changed");
}
void private_range(MemoryView memory, std::uintptr_t at, std::size_t size, std::uintptr_t allocation) {
    require(readable_range(memory, at, size, RangeUse::private_code), "Fervidex chain is not private executable memory");
    const auto end = at + size;
    for (auto cursor = at; cursor < end;) {
        MemoryRegion region{};
        require(memory.query && memory.query(memory.context, cursor, region) &&
            contains(region.begin, region.size, cursor, 1) && region.allocation == allocation &&
            region.kind == MemoryKind::private_memory && region.committed && region.readable &&
            region.executable && !region.guarded, "Fervidex chain allocation changed");
        cursor = (std::min)(end, region.begin + region.size);
    }
}
struct PrivateCopy {
    std::uintptr_t address{}, allocation{};
    std::vector<std::byte> bytes;
};
PrivateCopy private_copy(MemoryView memory, std::uintptr_t at, std::size_t size) {
    MemoryRegion region{};
    require(memory.query && memory.query(memory.context, at, region), "Fervidex chain allocation unavailable");
    private_range(memory, at, size, region.allocation);
    PrivateCopy result{at, region.allocation, std::vector<std::byte>(size)};
    require(read_bytes(memory, at, result.bytes), "Fervidex chain read failed");
    private_range(memory, at, size, result.allocation);
    return result;
}
struct Chain {
    std::vector<std::byte> incoming;
    PrivateCopy relay, stub, original;
};
struct Foreign {
    std::array<std::vector<std::byte>, foreign_pins.size()> bodies;
    std::array<Chain, 2> chains;
};
Foreign foreign(MemoryView memory, ImageRange game, ImageRange module) {
    Foreign result;
    // Independently decoded immediate/RIP operands all fit below these two
    // eight-byte object-pointer slots, the highest selected extents. This also
    // bounds the nine direct external entries and exact referenced strings;
    // it does not read their contents, traverse the objects or admit callees.
    (void)address(module, 0x27A5E0, 16);
    require(game.base + game.size <= module.base || module.base + module.size <= game.base,
            "Fervidex and game images overlap");
    for (std::size_t i = 0; i < foreign_pins.size(); ++i) {
        const auto pin = foreign_pins[i];
        auto& body = result.bodies[i];
        body = copy(memory, module, pin.begin, pin.end - pin.begin);
    }
    for (std::size_t i = 0; i < game_bindings(game).category_request_sites_contract.foreign_sites.size(); ++i) {
        auto& chain = result.chains[i];
        const auto at = address(game, game_bindings(game).category_request_sites_contract.foreign_sites[i], 5);
        const auto target = address(game, game_bindings(game).category_request_sites_contract.foreign_targets[i], 1);
        chain.incoming = copy(memory, game, game_bindings(game).category_request_sites_contract.foreign_sites[i], 5);
        if (prefix(chain.incoming, originals[i])) continue;
        require(chain.incoming[0] == std::byte{0xE9}, "unknown Fervidex incoming branch");
        const auto relay = relative(at, field<std::int32_t>(chain.incoming, 1));
        chain.relay = private_copy(memory, relay, 14);
        require(prefix(chain.relay.bytes, indirect), "Fervidex relay layout changed");
        const auto stub = field<std::uintptr_t>(chain.relay.bytes, 6);
        chain.stub = private_copy(memory, stub, 391);
        hash(std::span(chain.stub.bytes).first(375), stub_sha);
        require(field<std::uintptr_t>(chain.stub.bytes, 375) == address(module, foreign_pins[i].begin, 1),
                "Fervidex callback target changed");
        const auto trampoline = field<std::uintptr_t>(chain.stub.bytes, 383);
        require(trampoline >= 0x10000 && trampoline <= UINTPTR_MAX - 24 && relay == trampoline + 10,
                "Fervidex trampoline/relay relationship changed");
        chain.original = private_copy(memory, trampoline, 24);
        const auto& bytes = chain.original.bytes;
        require(chain.original.allocation == chain.relay.allocation &&
                std::equal(chain.relay.bytes.begin(), chain.relay.bytes.end(), bytes.begin() + 10) &&
                bytes[0] == std::byte{0xE8} && relative(trampoline, field<std::int32_t>(bytes, 1)) == target &&
                bytes[5] == std::byte{0xE9} && relative(trampoline + 5, field<std::int32_t>(bytes, 6)) == at + 5,
                "Fervidex displaced call or native continuation changed");
    }
    return result;
}
void normalize(std::span<std::byte> bytes, std::size_t offset, const Chain& chain, std::size_t kind) {
    require(offset <= bytes.size() && 5 <= bytes.size() - offset && chain.incoming.size() == 5 &&
        std::equal(chain.incoming.begin(), chain.incoming.end(), bytes.begin() + offset),
        "Fervidex entry changed across the enclosing body copy");
    for (std::size_t i = 0; i < 5; ++i) bytes[offset + i] = std::byte{originals[kind][i]};
}
struct Observation {
    std::array<std::vector<std::byte>, build_bindings::CategoryRequestSitesContract{}.pins.size()> bodies;
    Foreign routes;
};
Observation observe(MemoryView memory, ImageRange game, ImageRange fv) {
    Observation result;
    // Manager leaf RIP operand: bound the complete slot without reading its value.
    (void)address(game, game_bindings(game).category_request_sites_contract.category_manager_rva, 8);
    for (std::size_t i = 0; i < game_bindings(game).category_request_sites_contract.pins.size(); ++i) {
        const auto pin = game_bindings(game).category_request_sites_contract.pins[i];
        result.bodies[i] = copy(memory, game, pin.begin, pin.end - pin.begin);
    }
    if (fv.base) {
        result.routes = foreign(memory, game, fv);
        normalize(result.bodies[2], game_bindings(game).category_request_sites_contract.foreign_sites[1] - game_bindings(game).category_request_sites_contract.pins[2].begin, result.routes.chains[1], 1);
        normalize(result.bodies[5], game_bindings(game).category_request_sites_contract.foreign_sites[0] - game_bindings(game).category_request_sites_contract.pins[5].begin, result.routes.chains[0], 0);
    } else require(!fv.size, "absent Fervidex has a nonempty range");
    return result;
}
} // namespace

CategoryRequestSites admit_category_request_sites(MemoryView memory, const ImageInfo& game,
        const std::optional<ImageInfo>& fv) {
    const auto range = fv ? fv->range : ImageRange{};
    if (fv) (void)address(range, 0, 1);
    auto observed = observe(memory, game.range, range);
    CategoryRequestSites result;
    result.game = game.range; result.fervidex = range;
    result.manager_getter = address(game.range, game_bindings(game.range).category_request_sites_contract.pins[4].begin, 8);
    for (std::size_t i = 0; i < result.entries.size(); ++i) {
        const auto pin = game_bindings(game.range).category_request_sites_contract.pins[i];
        result.entries[i] = {address(game.range, pin.begin, pin.end - pin.begin), std::move(observed.bodies[i]),
            address(game.range, game_bindings(game.range).category_request_sites_contract.captures[i], 5), address(game.range, game_bindings(game.range).category_request_sites_contract.cancels[i], 1), address(game.range, game_bindings(game.range).category_request_sites_contract.callers[i], 1)};
    }
    return result;
}
} // namespace point_zero::native
