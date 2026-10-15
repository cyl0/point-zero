#include "folder_art_contract.hpp"

namespace point_zero::native {
FolderArtCalls admit_folder_art_calls(MemoryView memory, const ImageInfo& utils) {
    FolderArtCalls result;
    result.package = reinterpret_cast<decltype(result.package)>(resolve_export(memory, utils, 56));
    result.global_id = reinterpret_cast<decltype(result.global_id)>(resolve_export(memory, utils, 68));
    result.global_info = reinterpret_cast<decltype(result.global_info)>(resolve_export(memory, utils, 70));
    result.package_info = reinterpret_cast<decltype(result.package_info)>(resolve_export(memory, utils, 71));
    result.test_flags = reinterpret_cast<decltype(result.test_flags)>(resolve_export(memory, utils, 5));
    return result;
}
} // namespace point_zero::native
