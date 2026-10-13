#include "game_major.hpp"
#include "files.hpp"

#include <Windows.h>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace point_zero::loader {
namespace {
template<class T> T read(std::uintptr_t at) {
    T result{};
    SIZE_T bytes{};
    if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(at), &result, sizeof(result), &bytes) || bytes != sizeof(result))
        throw std::runtime_error("game image header unavailable");
    return result;
}

std::array<std::byte, 8> database_header(std::uintptr_t base,
    const IMAGE_OPTIONAL_HEADER64& image, const std::string& path) {
    const auto address = [&](std::uint32_t rva, std::size_t size) {
        if (rva >= image.SizeOfImage || size > image.SizeOfImage - rva)
            throw std::runtime_error("database AVS import outside game image");
        return base + rva;
    };
    const auto imports = image.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (image.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_IMPORT || !imports.VirtualAddress ||
        imports.Size < sizeof(IMAGE_IMPORT_DESCRIPTOR))
        throw std::runtime_error("database AVS imports unavailable");
    (void)address(imports.VirtualAddress, imports.Size);
    std::array<std::uintptr_t, 3> targets{};
    constexpr std::array<unsigned, 3> ordinals{79, 82, 86};
    for (std::size_t offset = 0; offset + sizeof(IMAGE_IMPORT_DESCRIPTOR) <= imports.Size;
         offset += sizeof(IMAGE_IMPORT_DESCRIPTOR)) {
        const auto entry = read<IMAGE_IMPORT_DESCRIPTOR>(base + imports.VirtualAddress + offset);
        if (!entry.Name) break;
        std::string provider;
        for (unsigned i = 0; i != 64; ++i) {
            const auto c = read<char>(address(entry.Name, i + 1) + i);
            if (!c) break;
            provider += c >= 'A' && c <= 'Z' ? char(c + ('a' - 'A')) : c;
        }
        if (provider != "avs2-core.dll") continue;
        if (!entry.OriginalFirstThunk || !entry.FirstThunk)
            throw std::runtime_error("database AVS import metadata unavailable");
        for (unsigned i = 0; i != 4096; ++i) {
            const auto bytes = (static_cast<std::size_t>(i) + 1) * sizeof(IMAGE_THUNK_DATA64);
            const auto thunk = read<IMAGE_THUNK_DATA64>(address(entry.OriginalFirstThunk, bytes) +
                bytes - sizeof(IMAGE_THUNK_DATA64));
            if (!thunk.u1.AddressOfData) break;
            if (!IMAGE_SNAP_BY_ORDINAL64(thunk.u1.Ordinal)) continue;
            for (std::size_t n = 0; n != ordinals.size(); ++n) {
                if (IMAGE_ORDINAL64(thunk.u1.Ordinal) != ordinals[n]) continue;
                if (targets[n]) throw std::runtime_error("ambiguous database AVS import");
                targets[n] = read<std::uintptr_t>(address(entry.FirstThunk, bytes) +
                    bytes - sizeof(IMAGE_THUNK_DATA64));
            }
        }
    }
    for (const auto target : targets) {
        MEMORY_BASIC_INFORMATION region{};
        if (!target || !VirtualQuery(reinterpret_cast<const void*>(target), &region, sizeof(region)) ||
            region.State != MEM_COMMIT || (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
            !(region.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
            throw std::runtime_error("database AVS filesystem import unavailable or not executable");
    }
    using Open = std::int32_t (*)(const char*, std::uint16_t, int);
    using Read = std::size_t (*)(std::int32_t, void*, std::size_t);
    using Close = void (*)(std::int32_t);
    const auto open = reinterpret_cast<Open>(targets[0]);
    const auto read_file = reinterpret_cast<Read>(targets[1]);
    const auto close = reinterpret_cast<Close>(targets[2]);
    const auto file = open(path.c_str(), 1, 0x1A4);
    if (file < 0) throw std::runtime_error("database AVS open failed: " + path + " result=" + std::to_string(file));
    struct Handle {
        std::int32_t value;
        Close close;
        ~Handle() { close(value); }
    } handle{file, close};
    std::array<std::byte, 8> header{};
    for (std::size_t offset = 0; offset != header.size();) {
        const auto got = read_file(file, header.data() + offset, header.size() - offset);
        if (!got || got > header.size() - offset)
            throw std::runtime_error("database AVS header read failed: " + path +
                " bytes=" + std::to_string(offset) + " result=" + std::to_string(got));
        offset += got;
    }
    return header;
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
    const auto header = database_header(base, optional, path);
    try { return music_database_major({reinterpret_cast<const char*>(header.data()), header.size()}); }
    catch (const std::exception& error) {
        throw std::runtime_error("database AVS header invalid: " + path + ": " + error.what());
    }
}
} // namespace point_zero::loader
