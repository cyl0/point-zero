#include "memory.hpp"

#include <windows.h>
#include <cstring>
#include <limits>

namespace point_zero::native {
namespace {
bool range(MemoryView memory, std::uintptr_t address, std::size_t length,
           const RangeUse* use, ImageRange image) noexcept {
    if (!memory.query || address < 0x10000 || !length ||
        length > (std::numeric_limits<std::uintptr_t>::max)() - address) return false;
    const bool image_use = use && (*use == RangeUse::image_read || *use == RangeUse::image_code);
    if (image_use && !contains(image.base, image.size, address, length)) return false;
    const auto end = address + length;
    std::uintptr_t allocation{};
    for (auto cursor = address; cursor < end;) {
        MemoryRegion region{};
        if (!memory.query(memory.context, cursor, region) ||
            !contains(region.begin, region.size, cursor, 1) ||
            !region.committed || !region.readable || region.guarded) return false;
        if (use) {
            if (!region.allocation || region.allocation > region.begin) return false;
            if (!allocation) allocation = region.allocation;
            if (allocation != region.allocation) return false;
            if (image_use) {
                if (region.kind != MemoryKind::image || region.allocation != image.base) return false;
                if (*use == RangeUse::image_code && !region.executable) return false;
            } else {
                if (region.kind != MemoryKind::private_memory) return false;
                if (*use == RangeUse::private_code && !region.executable) return false;
                if (*use == RangeUse::private_writable && (!region.writable || region.executable)) return false;
            }
        }
        const auto next = region.begin + region.size; // contains checked overflow.
        cursor = next < end ? next : end;
    }
    return true;
}

bool query_process(void*, std::uintptr_t address, MemoryRegion& out) noexcept {
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) != sizeof(info)) return false;
    const auto protection = info.Protect & 0xFFU;
    const bool readable = protection == PAGE_READONLY || protection == PAGE_READWRITE ||
        protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READ ||
        protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
    const bool writable = protection == PAGE_READWRITE || protection == PAGE_WRITECOPY ||
        protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
    const bool executable = protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
        protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
    const auto kind = info.Type == MEM_IMAGE ? MemoryKind::image :
        info.Type == MEM_PRIVATE ? MemoryKind::private_memory :
        info.Type == MEM_MAPPED ? MemoryKind::mapped : MemoryKind::unknown;
    out = {reinterpret_cast<std::uintptr_t>(info.BaseAddress),
           reinterpret_cast<std::uintptr_t>(info.AllocationBase), info.RegionSize, kind,
           info.State == MEM_COMMIT, readable, writable, executable, (info.Protect & PAGE_GUARD) != 0};
    return true;
}
bool copy_process(void*, std::uintptr_t address, std::span<std::byte> out) noexcept {
#if defined(_MSC_VER)
    __try { std::memcpy(out.data(), reinterpret_cast<const void*>(address), out.size()); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
#else
    SIZE_T count{};
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address),
                             out.data(), out.size(), &count) && count == out.size();
#endif
}
} // namespace

bool contains(std::uintptr_t begin, std::size_t size, std::uintptr_t address, std::size_t length) noexcept {
    return size <= (std::numeric_limits<std::uintptr_t>::max)() - begin &&
        address >= begin && address - begin <= size && length <= size - (address - begin);
}
bool readable_range(MemoryView memory, std::uintptr_t address, std::size_t length,
                    RangeUse use, ImageRange image) noexcept {
    return range(memory, address, length, &use, image);
}
bool read_bytes(MemoryView memory, std::uintptr_t address, std::span<std::byte> out) noexcept {
    if (out.empty()) return true;
    return memory.read && range(memory, address, out.size(), nullptr, {}) &&
        memory.read(memory.context, address, out);
}
MemoryView current_process_memory() noexcept { return {nullptr, copy_process, query_process}; }
} // namespace point_zero::native
