#pragma once

#include "image.hpp"

namespace point_zero::native {
struct CategoryRestoreCalls {
    std::uintptr_t (__fastcall* current)(std::uintptr_t navigation){};
    std::uintptr_t (__fastcall* restore)(std::uintptr_t selection, int category, int song,
                                        int p1, int p2, std::uint8_t force_open, int extra){};
};

// Restore's return value is not status; callers retain destination ownership.
CategoryRestoreCalls admit_category_restore_contract(MemoryView, const ImageInfo& game);
} // namespace point_zero::native
