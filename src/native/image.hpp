#pragma once

#include "memory.hpp"
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace point_zero::native {
struct ImageDirectory { std::uint32_t rva{}, size{}; };
struct ImageInfo {
    ImageRange range;
    std::array<ImageDirectory, 16> directories{};
    std::uint32_t timestamp{}, entry_rva{};
};

ImageInfo inspect_image(MemoryView, std::uintptr_t base);

std::uintptr_t resolve_export(MemoryView, const ImageInfo&, std::uint16_t ordinal);
std::vector<std::byte> read_code(MemoryView, ImageRange, std::uint32_t rva, std::size_t size);

// Holds an existing module load reference; never loads a new file or runs its
// entry point. Keep the lease until every owned native callback has retired.
class ModuleLease {
public:
    explicit ModuleLease(std::wstring_view name);
    ~ModuleLease();
    ModuleLease(const ModuleLease&) = delete;
    ModuleLease& operator=(const ModuleLease&) = delete;
    ModuleLease(ModuleLease&&) noexcept;
    ModuleLease& operator=(ModuleLease&&) noexcept;
    const ImageInfo& image() const noexcept { return image_; }
private:
    void* handle_{};
    ImageInfo image_;
};
} // namespace point_zero::native
