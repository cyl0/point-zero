#include "category_vector_contract.hpp"

#include <array>
#include <stdexcept>

namespace point_zero::native {
namespace {
// Complete extents and call/ABI evidence remain reproducible in retained-PE
// tests. Runtime admission checks only the two required callable entries.
constexpr std::array<std::uint32_t, 2> entries{0x6387B0, 0x611B20};

// Grow RCX=Vector*, RDX=old position, R8=address of one category pointer.
// It saves the insertion index (position-begin)/8, chooses capacity from
// max(old_capacity+old_capacity/2, old_size+1), copies *R8 into the new slot,
// then copies the prefix/suffix and frees the old allocation. 63897D/638980
// return new_begin+8*saved_index. It does not validate spare capacity or input
// pointers. Stock append 638A3F..638A57 calls it ONLY when end==capacity;
// otherwise 638A45/638A48 write the element and advance end by eight.
// The publication owner must follow that path, not call grow on every append.
//
// Destroy RCX=Vector*. If begin!=0, it frees then zeros all three fields.
// If begin==0, it returns without changing end/capacity: the caller must supply
// a well-formed empty triple. There is no category dereference/destruction.
// Both operations use the native allocator's >=1000h-byte path: aligned data
// stores allocation base at data[-8], validates data-base-8<=1Fh, and uses
// capacity bytes+27h for the free. Do not mix host allocations with this ABI.
//
// Statically decoded external dependencies (not runtime body pins):
// B1BE18 allocation, B505B0 byte copy, B1BE54 sized delete, B77434 invalid-
// parameter/error path; 638B80 vector-length error (calls AEE318). 6389B0 is
// the eight-byte helper clearing a stack bookkeeping byte and returning RCX.
// Grow's EH metadata uses B50458 and CB6420; catch BD5BA0..BD5BCE invokes
// 638BA0 to release its temporary allocation then B4F5A0 to rethrow. These
// helpers, handler metadata, CRT targets and heap state are external, not a
// recursively certified runtime or a promise that allocation cannot throw.
// Failure cleanup/retention remains the caller's responsibility.
//
// Read-only layout evidence, NOT additional admission targets or mutable reads:
// LEVEL ctor 63BC70..63BE23: 63BD28 resolves CB6B10, 63BD2F installs the vtable,
// 63BD32 stores DWORD1 at group+8 (group identity). 63BCF9 + 63BD10/63BD15
// construct two 18h-byte vectors at +78h, hence SP +78h and DP +90h. 63BD21
// zeros the two DWORD cursors at +A8h/+ACh. Native accessor 611CA4..611CB4
// returns group+A8h+4*style after checking style<2; 611CD7..611CEA and
// 638A25..638A3B use +78h+18h*style. Group destructor 611B92..611BA6 invokes
// 611B20 twice over those vectors. CB6B10 has four method slots (through
// CB6B30 exclusive), not an indefinitely traversable pointer table.
// No live group identity/lifetime, publication or ownership is proved here.

void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
void check_entry(MemoryView memory, ImageRange image, std::uint32_t entry) {
    require(image.base >= 0x10000 && contains(image.base, image.size, image.base, image.size) &&
            entry < image.size,
            "category vector contract entry outside selected image");
    require(readable_range(memory, image.base + entry, 1, RangeUse::image_code, image),
            "category vector contract image ownership/protection changed");
}
} // namespace

CategoryVectorCalls admit_category_vector_contract(MemoryView memory, const ImageInfo& game) {
    static_assert(sizeof(std::uintptr_t) == 8 && sizeof(category_layout::Vector) == 24);
    for (const auto entry : entries) check_entry(memory, game.range, entry);
    return {
        reinterpret_cast<decltype(CategoryVectorCalls::grow)>(game.range.base + 0x6387B0),
        reinterpret_cast<decltype(CategoryVectorCalls::destroy)>(game.range.base + 0x611B20),
    };
}
} // namespace point_zero::native
