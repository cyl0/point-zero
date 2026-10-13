#include "build_bindings.hpp"
#include "list_title_contract.hpp"

#include <cstdio>
#include <stdexcept>
#include <vector>
#include <utility>

namespace point_zero::native {
namespace {
using Pin = build_bindings::ListTitleContract::Pin;


// Evidence: retained 03_bm2dx/binaries/LDJ-010-2026081900/bm2dx.dll,
// inspected as PE data. Half-open ranges exclude following INT3 padding;
// neither body nor the width constant overlaps a base relocation.
//
// Complete title [5F8AC0,5F8E2A), disk [5F7EC0,5F822A), 874 bytes.
// RUNTIME_FUNCTION at BAE47D0 gives this entire extent, unwind F19FD0
// (EHANDLER|UHANDLER, not CHAININFO). Traversing all .pdata chains finds
// no descendants. RET at 5F8E29; next independent function at 5F8E30.
// Unwind metadata establishes fixture extent, not a runtime admission gate.
//
// Complete background caller [5F7BF0,5F7FD5), 997 bytes; same pin as
// list_hook_contract.cpp. Five records [BAE468C,BAE46C8) end at 5F7C1C,
// 5F7C37,5F7DCF,5F7FC4,5F7FD5; unwind F19ECC,F19EE0,F19F04,F19F1C,
// F19F2C; parents by index: -,0,1,1,0. Next function is 5F7FE0.
// RSI receives RCX at 5F7C10 and retains the owner through CALL 5F7E75
// to 5F8AC0 (return 5F7E7A). RCX=RSI, EDX=x, R8D=y, XMM3=alpha,
// outgoing [RSP+20]=int layer (input+1); at title entry it is [RSP+28].
// Title saves alpha in XMM6, y in R15D, x in R13D, owner in RSI (5F8B07),
// layer in R12D from [RBP+160]. No RSI write follows before COMISS.
//
// Both native bitmap paths are inside the full title pin: emit at 5F8BB1/
// 5F8C1E, native scale calls at 5F8BDA/5F8C4C, then jumps to 5F8DFF.
// Text measures [RSI] via 5E9410 at 5F8DAB. MOVSS XMM1,[RIP+6B33F0]
// at 5F8DB0 loads RVA CAC1A8 (disk CAADA8), bits 43FF0000 = 510.0f.
// 5F8DB8: COMISS XMM0,XMM1; JBE +10h to 5F8DCD. Oversize text divides
// XMM1 by XMM0, stores scale and sets flags before native text drawing.
// Only this inner comparison is returned for a scoped width hook; the
// global constant must remain stock, read-only and non-executable.
// Research references: candidate/source/src/list_ux_runtime.inl and
// list_ux_native_pins.inl under research_v1_playlister51_20261001.



constexpr std::uint32_t width_bits = 0x43FF0000;

void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }

std::uintptr_t address(MemoryView memory, ImageRange image, std::uint32_t rva,
                       std::size_t size, RangeUse use) {
    require(contains(image.base, image.size, image.base, image.size) &&
            rva <= image.size && size <= image.size - rva,
            "list title contract range outside selected image");
    const auto at = image.base + rva;
    require(readable_range(memory, at, size, use, image),
            "list title contract allocation or protection changed");
    return at;
}

std::vector<std::byte> code(MemoryView memory, ImageRange image, Pin pin) {
    const auto at = address(memory, image, pin.rva, pin.size, RangeUse::image_code);
    std::vector<std::byte> bytes(pin.size);
    require(read_bytes(memory, at, bytes), "list title contract body is unreadable");
    // read_bytes checks readability only; recheck image ownership/execution.
    (void)address(memory, image, pin.rva, pin.size, RangeUse::image_code);
    return bytes;
}

std::uintptr_t width_address(MemoryView memory, ImageRange image) {
    const auto at = address(memory, image, game_bindings(image).list_title_contract.width_rva, sizeof(width_bits), RangeUse::image_read);
    const auto end = at + sizeof(width_bits);
    // RangeUse::image_read permits writable images; this specific stock
    // constant does not. Check every region, including a split final byte.
    for (auto cursor = at; cursor < end;) {
        MemoryRegion region{};
        require(memory.query(memory.context, cursor, region) &&
                contains(region.begin, region.size, cursor, 1) &&
                region.kind == MemoryKind::image && region.allocation == image.base &&
                region.allocation <= region.begin && region.committed && region.readable &&
                !region.writable && !region.executable && !region.guarded,
                "list title stock width is not read-only image data");
        const auto next = region.begin + region.size;
        cursor = next < end ? next : end;
    }
    return at;
}

void stock_width(MemoryView memory, ImageRange image) {
    std::uint32_t bits{};
    require(read_value(memory, width_address(memory, image), bits),
            "list title stock width is unreadable");
    (void)width_address(memory, image);
    require(bits == width_bits, "list title stock width changed");
}
} // namespace

ListTitleSite admit_list_title_site(MemoryView memory, const ImageInfo& game) {
    auto body = code(memory, game.range, game_bindings(game.range).list_title_contract.title);
    (void)code(memory, game.range, game_bindings(game.range).list_title_contract.background);
    stock_width(memory, game.range);
    return {game.range.base + game_bindings(game.range).list_title_contract.title.rva, std::move(body), game.range.base + game_bindings(game.range).list_title_contract.title_background_return_rva,
            game_bindings(game.range).list_title_contract.title_width_end_rva - game_bindings(game.range).list_title_contract.title_entry_rva};
}

// Limits: observations are not atomic or a guarantee of future memory.
// Construction/arm must recheck the owned title body; the caller and stock
// width must remain stable through use. This pins the native bitmap/text
// instructions, not their transitive callees, imported/virtual targets,
// other constants/strings, exception handlers, or live row/resource state.
// Caller/thread/permit scoping and hook ownership belong to ListTitleHook.
} // namespace point_zero::native
