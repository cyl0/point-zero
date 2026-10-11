#pragma once

#include "memory.hpp"

namespace point_zero::native::detail {
// Scoped initializer view for PE metadata inspection. The shared image reader
// checks field values but not selected-image ownership after its final copy.
// This context must outlive the synchronous inspection; never retain its view.
struct MetadataView {
    MemoryView source;
    ImageRange image;
    static bool read(void* context, std::uintptr_t at, std::span<std::byte> out) noexcept {
        const auto& self = *static_cast<MetadataView*>(context);
        return self.source.read &&
            readable_range(self.source, at, out.size(), RangeUse::image_read, self.image) &&
            self.source.read(self.source.context, at, out) &&
            readable_range(self.source, at, out.size(), RangeUse::image_read, self.image);
    }
    static bool query(void* context, std::uintptr_t at, MemoryRegion& out) noexcept {
        const auto& self = *static_cast<MetadataView*>(context);
        return self.source.query && self.source.query(self.source.context, at, out);
    }
    MemoryView view() noexcept { return {this, read, query}; }
};
} // namespace point_zero::native::detail
