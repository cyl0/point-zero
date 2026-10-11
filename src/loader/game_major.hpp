#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace point_zero::loader {
// Read the selected database path from the running game's initialized data.
// All occurrences must name one file; stale neighboring banks are irrelevant.
std::string music_database_path(std::string_view image_data);
std::uint32_t music_database_major(std::string_view header);
// Resolve /data relative to the admitted game root. No datecode cutoff table,
// generation-version inference, game function calls, hooks or fixed RVAs.
std::uint32_t detect_game_major(const std::filesystem::path& runtime);
} // namespace point_zero::loader
