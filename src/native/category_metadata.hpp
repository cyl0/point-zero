#pragma once

#include "category_storage.hpp"

#include <vector>

namespace point_zero::native {
struct CategoryMetadataSite {
    std::uintptr_t address{};
    std::vector<std::byte> body; // Complete admitted implementation, not a jump stub.
};
struct CategoryMetadataEntry { int id{}; CategoryDefinition definition; };

// One fixed set of private LEVEL definitions. The admission layer supplies
// either the stock entry or the admitted Playlister implementation reached by
// that entry. Unknown IDs continue through the supplied native implementation.
// This class never discovers hooks or infers ownership from observed code.
class CategoryMetadata final {
public:
    // Prepared entries have strictly increasing IDs in230..999 (at most144),
    // difficulty_locked=1, and readable null-terminated strings up to255 bytes.
    // Strings are copied, including the distinction between null and empty.
    CategoryMetadata(MemoryView, const CategoryMetadataSite&,
                     std::span<const CategoryMetadataEntry>);
    ~CategoryMetadata() noexcept;
    CategoryMetadata(const CategoryMetadata&) = delete;
    CategoryMetadata& operator=(const CategoryMetadata&) = delete;
    void arm(); // One attempt. Caller owns admission, serialization and publication.
    bool intact() const noexcept;
    const CategoryDefinition* definition(int id) const noexcept;

    // Once enabling is attempted, copied definitions, strings and native forwarding
    // remain valid for process lifetime. No callback/context points into a bank
    // owner and no shutdown hook disables this mapping. Native consumers may
    // still hold a definition pointer after a lookup returns. Feature ON/OFF
    // controls publication, not whether an existing private ID has metadata.
    // During CategoryStorage::build only, its exact manager/ID TLS definition
    // supplies that operation's capacity. Otherwise the immutable definition
    // has capacity zero and is shared by both styles like native metadata.
private:
    struct State;
    State* state_{};
};
} // namespace point_zero::native
