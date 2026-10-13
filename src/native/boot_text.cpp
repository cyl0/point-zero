#include "build_bindings.hpp"
#include "boot_text.hpp"

#include <windows.h>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace point_zero::native {
namespace {
// Retained LDJ2026081900: formatted boot text helper [5EB470,5EB50C).
// Capture [93F629,93F63C) in boot draw [93F430,93F655), after OmniFix's
// separate 93F5C4 capture. Displace only the nine-byte stack argument MOV;
// the following footer CALL stays in native code with native unwind coverage.


constexpr std::array capture{
    std::byte{0x48}, std::byte{0xC7}, std::byte{0x44}, std::byte{0x24}, std::byte{0x20},
    std::byte{}, std::byte{}, std::byte{}, std::byte{}, std::byte{0xE8},
    std::byte{0xD9}, std::byte{0xBE}, std::byte{0xCA}, std::byte{0xFF}, std::byte{0x4C},
    std::byte{0x8D}, std::byte{0x5C}, std::byte{0x24}, std::byte{0x50}};

void require(bool okay, const char* message) {
    if (!okay) throw std::runtime_error(message);
}
auto code(MemoryView memory, ImageRange image) {
    require(game_bindings(image).boot_text.capture_rva <= image.size && capture.size() <= image.size - game_bindings(image).boot_text.capture_rva,
            "boot text range outside selected game");
    const auto at = image.base + game_bindings(image).boot_text.capture_rva;
    std::array<std::byte, capture.size()> bytes{};
    require(readable_range(memory, at, bytes.size(), RangeUse::image_code, image) &&
            read_bytes(memory, at, bytes) &&
            std::equal(bytes.begin(), bytes.end(), capture.begin()),
            "boot text native code changed");
    return bytes;
}
} // namespace

std::atomic<BootText*> BootText::owner_{};

// Native loop [93F494,93F5C4): eight rows from y112 in 32-pixel steps.
// OmniFix's setup_boot_text_hook uses the next row, y368, when loaded.
BootText::BootText(MemoryView memory, ImageRange game, std::string version)
    : row_y_(112 + (GetModuleHandleW(L"omnifix.dll") ? 9 : 8) * 32),
      version_(std::move(version)) {
    require(!version_.empty(), "boot text release version unavailable");
    require(game.base >= 0x10000 && contains(game.base, game.size, game.base, game.size) &&
            game_bindings(game).boot_text.renderer_rva <= game.size && 156 <= game.size - game_bindings(game).boot_text.renderer_rva &&
            readable_range(memory, game.base + game_bindings(game).boot_text.renderer_rva, 156, RangeUse::image_code, game),
            "boot text renderer outside selected executable game image");
    const auto before = code(memory, game);
    render_ = reinterpret_cast<Render>(game.base + game_bindings(game).boot_text.renderer_rva);
    patch_ = std::make_unique<detail::PlayedMidPatch>(memory, game.base + game_bindings(game).boot_text.capture_rva, before, draw);
}

void BootText::arm() {
    BootText* empty{};
    require(owner_.compare_exchange_strong(empty, this, std::memory_order_acq_rel),
            "boot text installation already attempted");
    // GameDriver retains this owner before enabling, even after a partial arm.
    patch_->enable();
    enabled_.store(true, std::memory_order_release);
}

void BootText::draw(safetyhook::Context&) noexcept {
    if (const auto owner = owner_.load(std::memory_order_acquire);
        owner && owner->enabled_.load(std::memory_order_acquire)) owner->render();
}

void BootText::render() noexcept {
    // Cached native row: y368 alone, y400 after OmniFix; columns stay native.
    // No lookups, allocation, I/O, hashing or shared-state mutation per frame.
    __try {
        render_(1, 60, row_y_, &color_, "POINT ZERO");
        render_(1, 300, row_y_, &color_, ":<color 00ffffff>%s</color>", version_.c_str());
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        stop(); // This optional decoration cannot throw across the mid-hook.
    }
}
} // namespace point_zero::native
