#pragma once

#include "category_requests.hpp"
#include "image.hpp"

namespace point_zero::native {
// Finite native CALL admission for LDJ-010-2026081900; throws on refusal.
// Reads seven function ranges once with executable image checks.
// Direct image operand extents are checked, but their contents/callees are
// not admitted. No runtime file, PE-directory, mutable-object or vtable reads.
// Ordinary function-body edits are allowed; pointers publish after range checks.
//
// save(store,id,map) takes a native map address (active set1 = settings()+48h),
// not the settings owner. clear(settings,1) CLEARS that map; it does not reload
// saved predicates. persistent returns AL only and accepts IDs210..214.
// save/clear/group return no usable status; sound's RAX is opaque, not success.
// The caller retains/serializes native objects and image lifetime and supplies
// valid style/group/map state. External callees, indirect calls and mutable
// globals are outside this finite contract. Range checks cannot prevent
// arbitrary concurrent or subsequent changes. Request-consumer hook admission
// and known foreign chains (including Fervidex) are separate contracts.
CategoryRequestCalls admit_category_request_calls(MemoryView, const ImageInfo& game);
} // namespace point_zero::native
