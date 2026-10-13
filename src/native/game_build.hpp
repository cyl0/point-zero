#pragma once

#include "image.hpp"
#include <string_view>

namespace point_zero::native {
struct GameBuildIdentity {
    std::uint32_t timestamp{}, image_size{}, entry_rva{};
    std::array<std::uint8_t, 16> codeview_guid{};
    std::uint32_t codeview_age{};
    bool operator==(const GameBuildIdentity&) const = default;
};

struct GameBuild {
    GameBuildIdentity identity;
    std::string_view datecode;
    const GameBindings* bindings;
};
const GameBuild& select_game_build(MemoryView, const ImageInfo&);

// Intrinsic identity of the selected loaded PE image.
GameBuildIdentity inspect_game_build(MemoryView, const ImageInfo&);
std::string_view supported_game_datecode(const GameBuildIdentity&) noexcept;
std::string_view check_game_build(MemoryView, const ImageInfo&);
} // namespace point_zero::native
