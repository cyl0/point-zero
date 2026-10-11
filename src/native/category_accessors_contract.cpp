#include "category_accessors_contract.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <iterator>
#include <stdexcept>
#include <vector>
#include <utility>

namespace point_zero::native {
namespace {
// Primary retained PE data (not whole-module hashes at runtime):
// 03_bm2dx/binaries/LDJ-010-2026081900/bm2dx.dll:
// c61b6dcb8894062e56d60da8ca90053b27f129e1a8e8da5e54457aa42602397d
// 09_outputs/research_v1_playlister51_20261001/playlister-5.1.0.dll:
// 717cea9a6f465cb4fc46b7dd93a3fd1b7fe07b5d365474bf666e91aeeb1969ba
// (identical DLL in the retained official release ZIP).
//
// Tests independently walk all unwind roots/chains and relocations. The eight
// native getters and seven PL bodies below each have ONE complete .pdata
// extent. Native flags=0; PL flags=2 (handler, not CHAININFO). No DIR64 fixups
// overlap these bodies. The style leaf [86F230,86F245) has no unwind record:
// selector at manager+2A424 ==1 returns 1; both remaining branches return 0.
// Its two RETs, including the 86F242 XOR/RET tail, are included.
//
// Native lookup calls style for selector3 and 9771E0 for the native ID bound;
// active reads manager+2A420 after style. Clear/rival suppress selector2, then
// index 168-byte summaries: clear+8+4*(2*diff+player), rival+48+4*(6*diff+rival).
// Badge bytes are KAC+3, Recommend+1, Shop+2, Weekly+0. Native category,
// summary and working style data remain caller-owned; admission is code only.
//
// Official PL 5.1 initializer and actual bodies establish these hook objects:
// 4259D -> 40AB0, 420C5 -> FE530 (lookup);
// 425CD -> 40B70, 420EE -> FE160 (active);
// 424ED -> 3FC00, its actual saved original at FE310 (clear, object FE2F0);
// 42182/421A0 -> 42680/FE0B0 (native Weekly; scanner slot100888);
// 421B9/421D7 -> 427A0/FE7F0 (native Recommend; PL featured);
// 421F0/4220E -> 428A0/FE000 (native Shop; PL tournament);
// 42227/42245 -> 429A0/FDF50 (native KAC; scanner slot100820).
// The scanner1EA0 classifies the native summary-byte offset, not discovery
// order: offset+3 selects KAC and offset+0 selects Weekly. The installer
// reads those slots at4222E/42189 respectively.
// Each actual implementation loads object+20 and checks object+28 before
// forwarding all native arguments via CALL RAX. IDs >=1000 retain PL's own
// route. Active's selected ID offset at PL+100860 must equal native2A420.
// Older 5.0.2 source is a locator only, not proof of this official DLL.
//
// SafetyHook 0.7 retained inline_hook.cpp establishes ONLY these two layouts:
// E9: stolen N bytes; game E9 to T+N+5 plus NOPs; T=original[N], E9 game+N,
//     FF25[0], absolute implementation (N+19 bytes).
// FF: game FF25[0], absolute implementation plus NOPs; T=original[N],
//     FF25[0], absolute game+N (N+14 bytes).
// No admitted stolen prefix has relative instructions. Active FF would steal
// CALL86F230 at +9; setup_ff explicitly REFUSES relative instructions, so that
// route is rejected rather than inventing a relocation scheme. Real PL entry
// prefixes are likewise nonrelative and suitable for the caller's own hook.
struct Pin {
    std::uint32_t begin, end;
    std::uint32_t pl_begin, pl_end;
    std::uint32_t object;
    unsigned near_stolen, far_stolen;
    std::array<unsigned char,16> original;
};
constexpr Pin pins[]{
    {0x86EE30,0x86EEA5,
     0x40AB0,0x40B67,0xFE530,5,15,
     {0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x20}},
    {0x86EEB0,0x86EEF5,
     0x40B70,0x40C52,0xFE160,6,0,
     {0x40,0x53,0x48,0x83,0xEC,0x20}},
    {0x86F570,0x86F5D6,
     0x3FC00,0x3FCDE,0xFE2F0,5,15,
     {0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,0x24,0x10,0x48,0x89,0x74,0x24,0x18}},
    {0x86F5E0,0x86F64D,
     0,0,0,5,0,{0x48,0x89,0x5C,0x24,0x08}},
    {0x86F750,0x86F78C,
     0x429A0,0x42A98,0xFDF50,5,16,
     {0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x20,0x48,0x63,0xDA,0x48,0x8B,0xF9}},
    {0x86F8A0,0x86F8DC,
     0x427A0,0x42898,0xFE7F0,5,16,
     {0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x20,0x48,0x63,0xDA,0x48,0x8B,0xF9}},
    {0x86FDB0,0x86FDEC,
     0x428A0,0x42998,0xFE000,5,16,
     {0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x20,0x48,0x63,0xDA,0x48,0x8B,0xF9}},
    {0x86FF00,0x86FF3C,
     0x42680,0x42778,0xFE0B0,5,16,
     {0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x20,0x48,0x63,0xDA,0x48,0x8B,0xF9}},
};
constexpr std::array<unsigned char,6> indirect{0xFF,0x25,0,0,0,0};
void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
template<class T> T value(const std::vector<std::byte>& bytes, std::size_t offset) {
    require(offset <= bytes.size() && sizeof(T) <= bytes.size() - offset, "category accessors field extent");
    T result{};
    std::memcpy(&result,bytes.data() + offset,sizeof(result));
    return result;
}
template<std::size_t N> bool prefix(const std::vector<std::byte>& bytes, std::size_t offset,
                                   const std::array<unsigned char,N>& expected, std::size_t size = N) {
    return size <= N && offset <= bytes.size() && size <= bytes.size() - offset &&
        std::equal(expected.begin(),expected.begin() + size,bytes.begin() + offset,
                   [](unsigned char a, std::byte b) { return a == std::to_integer<unsigned char>(b); });
}
bool nops(const std::vector<std::byte>& bytes, unsigned begin, unsigned end) {
    return begin <= end && end <= bytes.size() &&
        std::all_of(bytes.begin() + begin,bytes.begin() + end,[](std::byte b) { return b == std::byte{0x90}; });
}
std::uintptr_t address(ImageRange image, std::uint32_t rva, std::size_t size) {
    require(image.base >= 0x10000 && contains(image.base,image.size,image.base,image.size) &&
            rva <= image.size && size <= image.size - rva, "category accessors image extent");
    return image.base + rva;
}
std::vector<std::byte> copy_image(MemoryView memory, ImageRange image, std::uint32_t rva,
                                  std::size_t size, RangeUse use) {
    const auto at = address(image,rva,size);
    require(readable_range(memory,at,size,use,image), "category accessors image ownership/protection");
    std::vector<std::byte> bytes(size);
    require(read_bytes(memory,at,bytes), "category accessors image read");
    require(readable_range(memory,at,size,use,image), "category accessors image changed during copy");
    return bytes;
}
std::uintptr_t relative(std::uintptr_t instruction, std::int32_t displacement) {
    require(instruction <= (std::numeric_limits<std::uintptr_t>::max)() - 5, "category accessors rel32 overflow");
    const auto next = instruction + 5;
    const auto delta = static_cast<std::int64_t>(displacement);
    if (delta < 0) {
        require(next >= static_cast<std::uint64_t>(-delta), "category accessors rel32 underflow");
        return next - static_cast<std::uintptr_t>(-delta);
    }
    require(static_cast<std::uint64_t>(delta) <= (std::numeric_limits<std::uintptr_t>::max)() - next,
            "category accessors rel32 target overflow");
    return next + static_cast<std::uintptr_t>(delta);
}
void private_range(MemoryView memory, std::uintptr_t at, std::size_t size, std::uintptr_t allocation) {
    require(readable_range(memory,at,size,RangeUse::private_code), "category accessors private executable range");
    const auto end = at + size;
    for (auto cursor = at; cursor < end;) {
        MemoryRegion region{};
        require(memory.query && memory.query(memory.context,cursor,region) &&
                contains(region.begin,region.size,cursor,1) && region.allocation == allocation &&
                region.kind == MemoryKind::private_memory && region.committed && region.readable &&
                region.executable && !region.guarded, "category accessors trampoline allocation changed");
        cursor = (std::min)(end,region.begin + region.size);
    }
}
struct Route {
    CategoryAccessorSite site;
    std::vector<std::byte> game, binding, saved, extent, trampoline, active_offset;
    std::uintptr_t allocation{};
};
Route observe_route(MemoryView memory, const ImageInfo& game, const std::optional<ImageInfo>& pl, const Pin& pin) {
    Route result;
    result.game = copy_image(memory,game.range,pin.begin,pin.end - pin.begin,RangeUse::image_code);
    const auto native = address(game.range,pin.begin,pin.end - pin.begin);
    if (prefix(result.game,0,pin.original,pin.near_stolen)) {
        result.site = {native,result.game};
        return result;
    }
    require(pl.has_value() && pin.pl_begin, "category accessors unsupported foreign entry");
    const bool near = result.game[0] == std::byte{0xE9} && nops(result.game,5,pin.near_stolen);
    const bool far = pin.far_stolen && prefix(result.game,0,indirect) && nops(result.game,14,pin.far_stolen);
    require(near || far, "category accessors unsupported Playlister patch shape");
    const auto stolen = near ? pin.near_stolen : pin.far_stolen;
    const auto size = stolen + (near ? 19U : 14U);
    const auto implementation = address(pl->range,pin.pl_begin,pin.pl_end - pin.pl_begin);
    require(game.range.base + game.range.size <= pl->range.base ||
            pl->range.base + pl->range.size <= game.range.base, "category accessors images overlap");
    result.site = {implementation,copy_image(memory,pl->range,pin.pl_begin,pin.pl_end - pin.pl_begin,RangeUse::image_code)};
    // Only target/destination, allocation address/size, trampoline size. Mutable
    // mutexes, hook byte vectors, allocator control blocks and playlists excluded.
    result.binding = copy_image(memory,pl->range,pin.object,16,RangeUse::image_read);
    result.saved = copy_image(memory,pl->range,pin.object + 0x20,16,RangeUse::image_read);
    result.extent = copy_image(memory,pl->range,pin.object + 0x48,8,RangeUse::image_read);
    require(value<std::uintptr_t>(result.binding,0) == native &&
            value<std::uintptr_t>(result.binding,8) == implementation, "category accessors Playlister binding changed");
    const auto trampoline = value<std::uintptr_t>(result.saved,0);
    require(value<std::uint64_t>(result.saved,8) == size && value<std::uint64_t>(result.extent,0) == size &&
            contains(trampoline,size,trampoline,size), "category accessors saved original extent");
    require(near ? relative(native,value<std::int32_t>(result.game,1)) == trampoline + stolen + 5 :
                   value<std::uintptr_t>(result.game,6) == implementation, "category accessors incoming route mismatch");
    MemoryRegion region{};
    require(memory.query && memory.query(memory.context,trampoline,region), "category accessors trampoline query");
    result.allocation = region.allocation;
    private_range(memory,trampoline,size,result.allocation);
    result.trampoline.resize(size);
    require(read_bytes(memory,trampoline,result.trampoline), "category accessors trampoline copy");
    private_range(memory,trampoline,size,result.allocation);
    require(prefix(result.trampoline,0,pin.original,stolen), "category accessors native stolen bytes changed");
    if (near) {
        require(result.trampoline[stolen] == std::byte{0xE9} &&
                relative(trampoline + stolen,value<std::int32_t>(result.trampoline,stolen + 1)) == native + stolen &&
                prefix(result.trampoline,stolen + 5,indirect) &&
                value<std::uintptr_t>(result.trampoline,stolen + 11) == implementation,
                "category accessors near continuation/relay changed");
    } else {
        require(prefix(result.trampoline,stolen,indirect) &&
                value<std::uintptr_t>(result.trampoline,stolen + 6) == native + stolen,
                "category accessors far original continuation changed");
    }
    if (pin.begin == 0x86EEB0) {
        result.active_offset = copy_image(memory,pl->range,0x100860,8,RangeUse::image_read);
        require(value<std::uint64_t>(result.active_offset,0) == 0x2A420, "category accessors PL active ID field changed");
    }
    return result;
}
struct Observation {
    std::array<Route,static_cast<unsigned>(CategoryAccessor::count)> routes;
    CategoryAccessorSite style;
};
Observation observe(MemoryView memory, const ImageInfo& game, const std::optional<ImageInfo>& pl) {
    Observation result;
    for (unsigned i = 0; i < result.routes.size(); ++i) result.routes[i] = observe_route(memory,game,pl,pins[i]);
    result.style = {address(game.range,0x86F230,21),copy_image(memory,game.range,0x86F230,21,RangeUse::image_code)};
    return result;
}
// Remaining dependencies: native range-test9771E0, PL mutex/exception routines
// 5C860/5C980/5C810, unwind handlers and mutable native/PL storage. Call-site
// bytes are pinned; transitive helper bodies/data are not admitted here.
} // namespace

CategoryAccessorSites admit_category_accessors_contract(
    MemoryView memory, const ImageInfo& game, const std::optional<ImageInfo>& playlister) {
    static_assert(sizeof(std::uintptr_t) == 8);
    static_assert(std::size(pins) == static_cast<unsigned>(CategoryAccessor::count));
    auto observed = observe(memory, game, playlister);
    CategoryAccessorSites result;
    for (unsigned i = 0; i < result.entries.size(); ++i) result.entries[i] = std::move(observed.routes[i].site);
    result.style = std::move(observed.style);
    return result;
}
} // namespace point_zero::native
