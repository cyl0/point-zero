#pragma once

#include "adapters.hpp"
#include "played_calls.hpp"
#include "cup_contract.hpp"

namespace point_zero::native {
PlayedNativeCalls admit_played_calls(MemoryView, const NativeModules&);
PlayedNativeCalls admit_played_calls(MemoryView, const NativeModules&, const CupAfp&);
} // namespace point_zero::native
