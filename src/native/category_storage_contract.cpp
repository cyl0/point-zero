#include "category_storage_contract.hpp"

#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace point_zero::native {
namespace {

// Source: retained 03_bm2dx/binaries/LDJ-010-2026081900/bm2dx.dll,
// SHA256 c61b6dcb8894062e56d60da8ca90053b27f129e1a8e8da5e54457aa42602397d.
// Retained-PE tests reproduce complete extents and native ABI evidence.
// Runtime admission checks required callable entries and rebased bindings;
// it does not copy/discard full bodies or certify transitive callees.
//
// RCX=this for ctor/dtor; ctor returns this, intentionally ignored by void ABI.
// Init RCX=this, EDX=id: capacity is signed int definition+38h, NOT argument 2.
// 5FCB2C/3C resize bars to N+3 and charts to N. Fresh vectors have end==capacity;
// subsequent shrink helpers preserve those exact lengths. The parent needs no
// second bar resize when N is validated, N+3 fits int and <=N inserts succeed.
// Neither ctor nor init writes chart_count (+0Ch); caller initializes it.
// Init has no reliable status return; intptr_t is ABI-compatible, not success.
// String assignment RCX=category+48h, RDX=text, R8=size returns that string*.
// Insert RCX points to five pointers: music*, category*, difficulty, two ints;
// EDX=difficulty, R8D=style, R9D=id. Only AL is a boolean; success increments
// +10h. 5F9900 sets both working and original difficulty pairs from *params[2].
//
// +90h is an embedded 10h auxiliary object, NOT a heap allocation. Constructor
// sets CACDA0, +98h=0, +9Ch=0. +A0h helper zeros +B8h/+C0h/+C8h vector. Init
// calls 68E150 ONLY for IDs 210..214; caller excludes these IDs. Destructor
// frees nonempty auxiliary vector FIRST and can release arbitrary shared refs
// through [vtable]/[vtable+8]. Empty auxiliary storage is a caller precondition,
// not inferred by this code contract. Dtor then resets +90h=CACDA0, frees chart,
// string and bar buffers, clears their triples/string length, restores SSO=15.
// It does NOT free the category backing or reset counts, IDs or +98h/+9Ch.
// Vector/string frees at >=1000h bytes recover allocation base from data[-8],
// require (data-base-8)<=1Fh, add 27h to size, then call B1BE54. No CRT closure.
constexpr std::array<std::uint32_t, 5> call_entries{
    0x5FBC10, 0x5FCAD0, 0x366B00, 0x86E170, 0x882BD0};
// Exactly the auxiliary table's COL prefix and SIX slots, ending before text
// at CACDD0. Binding equality is rebased; rendering/deleting slot bodies are
// not used/admitted by private construction. The finite 24-byte COL identifies
// this table; its type/class descriptor graph is deliberately not traversed.
constexpr std::array<std::uint32_t, 7> auxiliary_bindings{
    0xE95E28, 0x5FC010, 0x6846C0, 0x5FD220, 0x5FD2A0, 0x5FC1D0, 0x5FD150};

void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
std::uintptr_t address(MemoryView memory, ImageRange image, std::uint32_t rva,
                       std::size_t size, RangeUse use) {
    require(image.base >= 0x10000 && contains(image.base, image.size, image.base, image.size) &&
            rva <= image.size && size <= image.size - rva,
            "category storage contract range outside selected image");
    const auto at = image.base + rva;
    require(readable_range(memory, at, size, use, image),
            "category storage contract allocation or protection changed");
    return at;
}
std::vector<std::byte> copy(MemoryView memory, ImageRange image, std::uint32_t rva,
                            std::size_t size, RangeUse use) {
    const auto at = address(memory, image, rva, size, use);
    std::vector<std::byte> bytes(size);
    require(read_bytes(memory, at, bytes), "category storage contract read failed");
    // read_bytes alone checks neither selected image allocation nor execution.
    (void)address(memory, image, rva, size, use);
    return bytes;
}
void bindings(MemoryView memory, ImageRange image) {
    const auto bytes = copy(memory, image, 0xCACD98,
                            auxiliary_bindings.size() * sizeof(std::uintptr_t), RangeUse::image_read);
    for (std::size_t i = 0; i < auxiliary_bindings.size(); ++i) {
        std::uintptr_t actual{};
        std::memcpy(&actual, bytes.data() + i * sizeof(actual), sizeof(actual));
        require(auxiliary_bindings[i] < image.size,
                "category storage auxiliary target outside selected image");
        // The method entries identify the vtable; private construction does
        // not call them. The COL is separately read above.
        if (!i) (void)address(memory, image, auxiliary_bindings[i], 1, RangeUse::image_read);
        require(actual == image.base + auxiliary_bindings[i],
                "category storage auxiliary binding changed");
    }
}
} // namespace

CategoryNativeCalls admit_category_storage_contract(MemoryView memory, const ImageInfo& game) {
    static_assert(sizeof(std::uintptr_t) == 8);
    for (const auto entry : call_entries) (void)address(memory, game.range, entry, 1, RangeUse::image_code);
    (void)address(memory, game.range, 0xE95E28, 24, RangeUse::image_read);
    bindings(memory, game.range);
    const auto base = game.range.base;
    CategoryNativeCalls result;
    result.construct = reinterpret_cast<decltype(result.construct)>(base + 0x5FBC10);
    result.initialize = reinterpret_cast<decltype(result.initialize)>(base + 0x5FCAD0);
    result.assign_string = reinterpret_cast<decltype(result.assign_string)>(base + 0x366B00);
    result.insert = reinterpret_cast<decltype(result.insert)>(base + 0x86E170);
    result.destroy = reinterpret_cast<decltype(result.destroy)>(base + 0x882BD0);
    result.auxiliary_vtable = base + 0xCACDA0;
    return result;
}

// Metadata resolver 86EF00 may be hooked by Playlister; separate caller-owned
// admission must prove its routing before ANY initialization. Likewise insert
// calls native candidate policy 893FA0; filter/hook admission owns that path.
// 5F9900's availability/style internals and temporary shared refs are engine
// dependencies, not category-owned allocations. Mutable singleton values,
// policy/lookup internals, CRT targets (including B1BE54 -> B1BB38), exception
// handlers and unused auxiliary virtual functions are outside this contract.
} // namespace point_zero::native
