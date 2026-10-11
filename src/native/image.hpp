#pragma once

#include "memory.hpp"
#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace point_zero::native {
struct ImageDirectory { std::uint32_t rva{}, size{}; };
struct ImageInfo {
    ImageRange range;
    // Import/export metadata is validated; other entries are copied for fixtures.
    std::array<ImageDirectory, 16> directories{};
};

// Only reads the selected loaded PE64 image. Malformed/inaccessible/changing
// identity, bounds or import/export metadata throw; section descriptions and
// unselected directory contents are not admission requirements.
ImageInfo inspect_image(MemoryView, std::uintptr_t base);

struct ImportReference {
    std::string provider, name;
    std::optional<std::uint16_t> ordinal;
    std::uintptr_t slot{}, target{};
};
ImportReference inspect_import(MemoryView, const ImageInfo&, std::uint32_t slot_rva);
struct ExportReference {
    std::uintptr_t address{};
    std::string forwarder; // Nonempty: address denotes this string, not executable code.
};
ExportReference inspect_export(MemoryView, const ImageInfo&, std::uint16_t ordinal);

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
