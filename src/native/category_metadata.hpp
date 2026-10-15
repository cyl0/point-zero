#pragma once

#include "category_storage.hpp"

#include <vector>

namespace point_zero::native {
struct CategoryMetadataSite {
    std::uintptr_t address{};
    std::vector<std::byte> body;
};
struct CategoryMetadataEntry { int id{}; CategoryDefinition definition; };

// Retained definition templates; runtime routing follows published bindings.
class CategoryMetadata final {
public:
    CategoryMetadata(MemoryView, const CategoryMetadataSite&,
                     std::span<const CategoryMetadataEntry>);
    ~CategoryMetadata() noexcept;
    CategoryMetadata(const CategoryMetadata&) = delete;
    CategoryMetadata& operator=(const CategoryMetadata&) = delete;
    void arm(); // One attempt. Caller owns admission, serialization and publication.
    bool intact() const noexcept;
    const CategoryDefinition* definition(int id) const noexcept;

    // Returned strings and definitions remain valid for process lifetime.
private:
    struct State;
    State* state_{};
};
} // namespace point_zero::native
