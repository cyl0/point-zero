#include "game_major.hpp"
#include "release.hpp"

#include <stdexcept>

namespace point_zero::loader {
std::string music_database_path(std::string_view data) {
    std::string selected;
    constexpr std::string_view prefix = "/data/info/";
    for (auto at = data.find(prefix); at != data.npos; at = data.find(prefix, at + prefix.size())) {
        const auto end = data.find('\0', at);
        if (end == data.npos || end - at > 80) continue;
        const auto path = data.substr(at, end - at);
        const auto slash = path.rfind('/');
        const auto file = path.substr(slash + 1);
        if (!file.starts_with("music_") || !file.ends_with(".bin")) continue;
        if (!valid_relative_path(path.substr(1))) throw std::runtime_error("invalid game database path");
        if (!selected.empty() && selected != path) throw std::runtime_error("ambiguous game database path");
        selected = path;
    }
    if (selected.empty()) throw std::runtime_error("game database path unavailable");
    return selected;
}
std::uint32_t music_database_major(std::string_view header) {
    if (header.size() != 8 || header.substr(0, 4) != "IIDX")
        throw std::runtime_error("invalid game database header");
    const auto u32 = [&](std::size_t at) {
        std::uint32_t value{};
        for (unsigned byte = 0; byte != 4; ++byte)
            value |= static_cast<std::uint32_t>(static_cast<unsigned char>(header[at + byte])) << (8U * byte);
        return value;
    };
    const auto major = u32(4);
    if (major == 0 || major > 999)
        throw std::runtime_error("invalid game database major");
    return major;
}
} // namespace point_zero::loader
