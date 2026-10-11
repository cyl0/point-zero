#pragma once
#include "list.hpp"
namespace point_zero::native {
// Read-only final-datecode call admission; no hook, native call or module load.
ListNativeCalls admit_list_calls(MemoryView, const NativeModules&);
} // namespace point_zero::native
