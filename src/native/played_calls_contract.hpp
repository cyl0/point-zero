#pragma once

#include "adapters.hpp"
#include "played_calls.hpp"
#include "cup_contract.hpp"

namespace point_zero::native {
// Finite, read-only LDJ-010-2026081900 admission for PlayedRenderer,
// ResultPlate and ResultNotes. Throws on invalid executable ranges or changed exports/selected bindings.
// Requires structurally inspected, retained module images; executes no native
// function, loads no module and installs no hook. Capture sites, stock bitmap
// forwarding, catalogue, resources and live owners have separate contracts.
PlayedNativeCalls admit_played_calls(MemoryView, const NativeModules&);
// Reuse the cup admission already performed by the composition owner.
PlayedNativeCalls admit_played_calls(MemoryView, const NativeModules&, const CupAfp&);
} // namespace point_zero::native
