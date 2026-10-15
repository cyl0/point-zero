#include "cup_contract.hpp"

namespace point_zero::native {
CupAfp admit_cup_calls(MemoryView memory, const ImageInfo& core, const ImageInfo& utils) {
    CupAfp result;
    result.valid = reinterpret_cast<decltype(result.valid)>(resolve_export(memory, core, 22));
    result.path = reinterpret_cast<decltype(result.path)>(resolve_export(memory, core, 103));
    result.path_relative = reinterpret_cast<decltype(result.path_relative)>(resolve_export(memory, core, 104));
    result.relative = reinterpret_cast<decltype(result.relative)>(resolve_export(memory, core, 106));
    result.attach = reinterpret_cast<decltype(result.attach)>(resolve_export(memory, core, 111));
    result.control = reinterpret_cast<decltype(result.control)>(resolve_export(memory, core, 114));
    result.property = reinterpret_cast<decltype(result.property)>(resolve_export(memory, core, 116));
    result.package = reinterpret_cast<decltype(result.package)>(resolve_export(memory, utils, 56));
    result.animation_info = reinterpret_cast<decltype(result.animation_info)>(resolve_export(memory, utils, 99));
    return result;
}
} // namespace point_zero::native
