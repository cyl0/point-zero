#include "game_major.hpp"
#include "files.hpp"

#include <Windows.h>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace point_zero::loader {
namespace {
template<class T> T read(std::uintptr_t at) {
    T result{};
    SIZE_T bytes{};
    if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(at), &result, sizeof(result), &bytes) || bytes != sizeof(result))
        throw std::runtime_error("game image header unavailable");
    return result;
}
}
std::uint32_t detect_game_major(const std::filesystem::path& runtime) {
    const auto root = std::filesystem::current_path();
    if (!std::filesystem::equivalent(runtime, root) && !std::filesystem::equivalent(runtime, root / L"modules"))
        throw std::runtime_error("POINT ZERO must be installed in the game root or modules directory");
    const auto game = GetModuleHandleW(L"bm2dx.dll");
    if (!game) throw std::runtime_error("game image unavailable for major detection");
    const auto base = reinterpret_cast<std::uintptr_t>(game);
    const auto dos = read<IMAGE_DOS_HEADER>(base);
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 || dos.e_lfanew > 1024 * 1024)
        throw std::runtime_error("invalid game image DOS header");
    const auto nt = base + static_cast<std::uintptr_t>(dos.e_lfanew);
    if (read<DWORD>(nt) != IMAGE_NT_SIGNATURE) throw std::runtime_error("invalid game image signature");
    const auto file = read<IMAGE_FILE_HEADER>(nt + sizeof(DWORD));
    const auto optional = read<IMAGE_OPTIONAL_HEADER64>(nt + sizeof(DWORD) + sizeof(file));
    if (file.Machine != IMAGE_FILE_MACHINE_AMD64 || optional.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        file.NumberOfSections == 0 || file.NumberOfSections > 96 ||
        file.SizeOfOptionalHeader != sizeof(optional) || optional.SizeOfImage > 512U * 1024U * 1024U)
        throw std::runtime_error("invalid game image layout");
    const auto sections = nt + sizeof(DWORD) + sizeof(file) + file.SizeOfOptionalHeader;
    std::string paths;
    std::size_t total{};
    for (unsigned i = 0; i != file.NumberOfSections; ++i) {
        const auto section = read<IMAGE_SECTION_HEADER>(sections + i * sizeof(IMAGE_SECTION_HEADER));
        // Only initialized, non-executable read-only data: no scan of the
        // large writable catalogue buffer or game code and no fixed layout.
        if (!(section.Characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA) ||
            !(section.Characteristics & IMAGE_SCN_MEM_READ) ||
            section.Characteristics & (IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_DISCARDABLE)) continue;
        const auto bytes = section.Misc.VirtualSize;
        if (section.VirtualAddress > optional.SizeOfImage || bytes > optional.SizeOfImage - section.VirtualAddress ||
            bytes > 16U * 1024U * 1024U || total + bytes > 32U * 1024U * 1024U)
            throw std::runtime_error("game image data limit");
        total += bytes;
        std::string data(bytes, '\0');
        SIZE_T got{};
        if (bytes && (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(base + section.VirtualAddress),
                                        data.data(), bytes, &got) || got != bytes))
            throw std::runtime_error("game image data unavailable");
        paths += data;
        paths += '\0';
    }
    const auto path = music_database_path(paths);
    FilePin database(root / std::filesystem::path(path.substr(1)));
    return music_database_major(database.read_prefix(8));
}
} // namespace point_zero::loader
