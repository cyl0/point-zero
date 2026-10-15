#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace point_zero::native {
enum class MemoryKind { unknown, image, private_memory, mapped };
struct MemoryRegion {
    std::uintptr_t begin{}, allocation{};
    std::size_t size{};
    MemoryKind kind{};
    bool committed{}, readable{}, writable{}, executable{}, guarded{};
};
struct MemoryView {
    void* context{};
    bool (*read)(void*, std::uintptr_t, std::span<std::byte>) noexcept{};
    bool (*query)(void*, std::uintptr_t, MemoryRegion&) noexcept{};
    bool operator==(const MemoryView&) const = default;
};
struct GameBindings;
extern const GameBindings august_bindings;
struct ImageRange {
    std::uintptr_t base{};
    std::size_t size{};
    const GameBindings* bindings = &august_bindings;
};
enum class RangeUse { image_read, image_code, private_code, private_writable };

// Checked half-open ranges, including overflow. No remote-process access.
bool contains(std::uintptr_t begin, std::size_t size,
              std::uintptr_t address, std::size_t length) noexcept;
// Checks every intersected region. Image uses require the selected allocation;
// private storage may start inside its allocation and span protection regions.
bool readable_range(MemoryView, std::uintptr_t, std::size_t, RangeUse,
                    ImageRange image = {}) noexcept;
bool read_bytes(MemoryView, std::uintptr_t, std::span<std::byte>) noexcept;
template<class T> bool read_value(MemoryView memory, std::uintptr_t address, T& value) noexcept {
    return read_bytes(memory, address, std::as_writable_bytes(std::span(&value, 1)));
}
// Observations inside an admitted native callback need a guarded copy, not a
// page query for every field. Writes/calls still require their owner's checks.
template<class T> bool read_field(MemoryView memory, std::uintptr_t address,
                                 std::size_t offset, T& value) noexcept {
    return memory.read && address >= 0x10000 && offset <= UINTPTR_MAX - address &&
        sizeof(T) <= UINTPTR_MAX - (address + offset) &&
        memory.read(memory.context, address + offset,
                    std::as_writable_bytes(std::span(&value, 1)));
}
MemoryView current_process_memory() noexcept;
bool callable_range(MemoryView, std::uintptr_t address, std::size_t length = 1) noexcept;
} // namespace point_zero::native
