#include "catalog_read.hpp"

#include <Windows.h>
#include <array>
#include <bit>
#include <cstring>
#include <stdexcept>

namespace point_zero::native {
namespace {
static_assert(std::endian::native == std::endian::little);
constexpr std::size_t header_size = 16, record_size = 2040;
void require(bool okay, const char* reason) {
    if (!okay) throw std::runtime_error(reason);
}
template<class T> T value(std::span<const std::byte> data, std::size_t offset) {
    require(offset <= data.size() && sizeof(T) <= data.size() - offset, "catalogue field outside snapshot");
    T result;
    std::memcpy(&result, data.data() + offset, sizeof(result));
    return result;
}
bool native_header(void* context, std::uintptr_t& result) noexcept {
    const auto function = static_cast<const CatalogGetters*>(context)->header;
    if (!function) return false;
    __try { result = reinterpret_cast<std::uintptr_t(__fastcall*)()>(function)(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
} // namespace

CapturedCatalog read_catalog(const CatalogAccess& access) {
    constexpr std::size_t snapshot_limit = 16 * 1024 * 1024;
    std::uintptr_t address{};
    require(access.memory.read && access.header, "incomplete catalogue access");
    require(access.header(access.header_context, address), "catalogue header getter failed");
    require(address >= 0x10000 && snapshot_limit <= UINTPTR_MAX - address, "invalid catalogue storage");
    std::array<std::byte, header_size> header{};
    require(access.memory.read(access.memory.context, address, header), "catalogue header unavailable");
    require(value<std::uint32_t>(header, 0) == 0x58444949 && value<std::uint32_t>(header, 4) == 34,
            "catalogue magic or major mismatch");
    const auto records = value<std::uint32_t>(header, 8);
    const auto id_limit = value<std::uint32_t>(header, 12);
    require(records && id_limit > 1 && id_limit <= 100000, "catalogue count or ID limit invalid");
    const auto records_offset = header_size + 4ULL * id_limit;
    require(records_offset <= snapshot_limit && records <= (snapshot_limit - records_offset) / record_size,
            "catalogue exceeds snapshot limit");
    const auto size = static_cast<std::size_t>(records_offset + record_size * records);
    std::vector<std::byte> snapshot(size);
    require(access.memory.read(access.memory.context, address, snapshot) &&
            std::memcmp(snapshot.data(), header.data(), header.size()) == 0, "catalogue changed during capture");
    std::uintptr_t observed{};
    require(access.header(access.header_context, observed) && observed == address,
            "catalogue header moved during capture");
    CapturedCatalog result;
    std::vector<bool> seen(id_limit);
    result.charts.reserve(static_cast<std::size_t>(records) * 10);
    for (std::uint32_t i = 0; i < records; ++i) {
        const auto offset = static_cast<std::size_t>(records_offset + record_size * i);
        const auto id = value<std::int32_t>(snapshot, offset + 0x67C);
        require(id > 0 && static_cast<std::uint32_t>(id) < id_limit && !seen[id], "invalid or duplicate registered song ID");
        seen[id] = true;
        require(value<std::int32_t>(snapshot, header_size + 4ULL * static_cast<unsigned>(id)) == static_cast<int>(i),
                "catalogue ID-index disagreement");
        for (int chart = 0; chart < 10; ++chart) {
            const int level = value<std::int8_t>(snapshot, offset + 0x3EC + chart);
            require(level >= 0 && level <= 12, "invalid native chart level");
            if (level) result.charts.push_back({{id, chart / 5, chart % 5}, level});
        }
    }
    return result;
}

CapturedCatalog read_current_catalog(CatalogGetters getters) {
    return read_catalog({current_process_memory(), &getters, native_header});
}
} // namespace point_zero::native
