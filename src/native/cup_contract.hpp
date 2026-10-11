#pragma once

#include "cup.hpp"

namespace point_zero::native {
// Admit the selected AFP cup exports and used property dispatch entries before
// loading/attaching resources. Throws on an unknown export/dispatch or invalid executable range. No calls,
// hooks or writes. This is not admission of the game/menu or shared bitmap hook.
CupAfp admit_cup_calls(MemoryView, const ImageInfo& core, const ImageInfo& utils);
} // namespace point_zero::native
