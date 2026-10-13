cmake_minimum_required(VERSION 3.28)
project(point_zero LANGUAGES C CXX)
# Build with VS 2022 C++ tools and a Windows SDK:
# cmake -S . -B build -G "Visual Studio 17 2022" -A x64
# cmake --build build --config Release --target point_zero_loader point_zero_updater point_zero_core
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "POINT ZERO requires Windows x64 and MSVC")
endif()
enable_language(ASM_MASM RC)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/experimental:deterministic>
    $<$<COMPILE_LANGUAGE:C,CXX>:/Brepro>
    "$<$<COMPILE_LANGUAGE:C,CXX>:/pathmap:${CMAKE_BINARY_DIR}=build>"
    "$<$<COMPILE_LANGUAGE:C,CXX>:/pathmap:${CMAKE_SOURCE_DIR}=.>")
add_link_options(/Brepro /INCREMENTAL:NO)
set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /Brepro")
include(cmake/Dependencies.cmake)
set(POINT_ZERO_RAPIDJSON_INCLUDE_DIR "${rapidjson_SOURCE_DIR}/include")
set(POINT_ZERO_NOTICES_RESOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/notices.rc")
configure_file(distribution/notices.rc.in "${POINT_ZERO_NOTICES_RESOURCE}" @ONLY)
set_property(SOURCE "${POINT_ZERO_NOTICES_RESOURCE}" PROPERTY OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/distribution/THIRD-PARTY-NOTICES.txt")

add_library(point_zero_ratings STATIC src/ratings/ratings.cpp)
target_include_directories(point_zero_ratings PUBLIC src)
target_include_directories(point_zero_ratings SYSTEM PRIVATE "${POINT_ZERO_RAPIDJSON_INCLUDE_DIR}")
target_compile_definitions(point_zero_ratings PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(point_zero_ratings PRIVATE yaml-cpp::yaml-cpp bcrypt)

function(point_zero_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:/W4;/WX;/permissive-;/utf-8;/EHsc>)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
    endif()
endfunction()
point_zero_warnings(point_zero_ratings)

add_library(point_zero_native_memory STATIC src/native/memory.cpp src/native/image.cpp src/native/game_build.cpp src/native/build_bindings.cpp)
target_include_directories(point_zero_native_memory PUBLIC src)
target_compile_definitions(point_zero_native_memory PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
point_zero_warnings(point_zero_native_memory)

add_library(point_zero_native_catalog STATIC src/native/catalog.cpp src/native/catalog_read.cpp)
target_link_libraries(point_zero_native_catalog PUBLIC point_zero_ratings point_zero_native_memory)
target_compile_definitions(point_zero_native_catalog PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
point_zero_warnings(point_zero_native_catalog)

add_library(point_zero_native_contract STATIC src/native/catalog_contract.cpp src/native/avs_read.cpp)
target_link_libraries(point_zero_native_contract PUBLIC point_zero_native_catalog point_zero_native_memory)
point_zero_warnings(point_zero_native_contract)

add_library(point_zero_native_assets STATIC src/native/assets.cpp src/native/asset_contract.cpp)
target_link_libraries(point_zero_native_assets PUBLIC point_zero_native_memory PRIVATE point_zero_ratings)
target_compile_definitions(point_zero_native_assets PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
point_zero_warnings(point_zero_native_assets)

add_library(point_zero_native_list_entry OBJECT src/native/list_entry.asm)
add_library(point_zero_native_sort_entry OBJECT src/native/sort_entry.asm)
add_library(point_zero_native_filter_entry OBJECT src/native/filter_entry.asm)
add_library(point_zero_native_hook STATIC src/native/catalog_hook.cpp
    src/native/inline_patch.cpp
    src/native/call_patch.cpp src/native/surface_hooks.cpp src/native/list_hook.cpp
    src/native/title_limit_patch.cpp src/native/list_title_hook.cpp
    src/native/played_mid_patch.cpp src/native/played_hooks.cpp src/native/sort_hook.cpp src/native/filter_hooks.cpp
    $<TARGET_OBJECTS:point_zero_native_list_entry>
    $<TARGET_OBJECTS:point_zero_native_sort_entry> $<TARGET_OBJECTS:point_zero_native_filter_entry>)
target_link_libraries(point_zero_native_hook PUBLIC point_zero_native_memory PRIVATE safetyhook::safetyhook point_zero_ratings)
target_include_directories(point_zero_native_hook PUBLIC include)
target_compile_features(point_zero_native_hook PRIVATE cxx_std_23)
target_compile_definitions(point_zero_native_hook PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
point_zero_warnings(point_zero_native_hook)

add_library(point_zero_native_state STATIC src/native/feature_state.cpp)
target_include_directories(point_zero_native_state PUBLIC src)
point_zero_warnings(point_zero_native_state)

add_library(point_zero_native_runtime STATIC src/core/runtime.cpp)
target_include_directories(point_zero_native_runtime PUBLIC include src)
target_link_libraries(point_zero_native_runtime PUBLIC point_zero_native_catalog point_zero_native_state)
point_zero_warnings(point_zero_native_runtime)

add_library(point_zero_native_driver STATIC src/native/game_driver.cpp src/native/boot_text.cpp)
target_link_libraries(point_zero_native_driver PUBLIC point_zero_native_runtime
    point_zero_native_contract point_zero_native_assets point_zero_native_hook
    PRIVATE safetyhook::safetyhook)
target_compile_definitions(point_zero_native_driver PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
point_zero_warnings(point_zero_native_driver)

add_library(point_zero_native_selection STATIC src/native/song_select.cpp)
target_link_libraries(point_zero_native_selection PUBLIC point_zero_native_catalog point_zero_native_memory)
point_zero_warnings(point_zero_native_selection)

add_library(point_zero_native_cup STATIC src/native/cup.cpp src/native/cup_layout.cpp
    src/native/cup_contract.cpp src/native/surface_contract.cpp)
target_include_directories(point_zero_native_cup PUBLIC include src)
target_link_libraries(point_zero_native_cup PUBLIC point_zero_native_selection point_zero_native_state)
target_compile_definitions(point_zero_native_cup PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
point_zero_warnings(point_zero_native_cup)

add_library(point_zero_native_list STATIC src/native/list.cpp src/native/list_chart.cpp
    src/native/list_layout.cpp src/native/list_contract.cpp src/native/list_hook_contract.cpp
    src/native/list_title_contract.cpp)
target_include_directories(point_zero_native_list PUBLIC include src)
target_link_libraries(point_zero_native_list PUBLIC point_zero_native_catalog
    point_zero_native_memory point_zero_native_state)
target_compile_definitions(point_zero_native_list PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
point_zero_warnings(point_zero_native_list)

add_library(point_zero_native_played STATIC src/native/played_chart.cpp src/native/played_layout.cpp src/native/stepup_contract.cpp
    src/native/played_contract.cpp src/native/played_calls_contract.cpp src/native/played_calls.cpp src/native/played.cpp
    src/native/result_plate.cpp src/native/result_notes.cpp)
target_include_directories(point_zero_native_played PUBLIC include src)
target_link_libraries(point_zero_native_played PUBLIC point_zero_native_catalog point_zero_native_memory
    point_zero_native_state PRIVATE point_zero_native_cup)
target_compile_definitions(point_zero_native_played PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
point_zero_warnings(point_zero_native_played)

add_library(point_zero_native_surfaces STATIC src/native/surfaces.cpp)
target_link_libraries(point_zero_native_surfaces PUBLIC point_zero_native_cup
    point_zero_native_list point_zero_native_played point_zero_native_hook)
target_compile_definitions(point_zero_native_surfaces PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
point_zero_warnings(point_zero_native_surfaces)

add_library(point_zero_native_sort STATIC src/native/difficulty_sort.cpp src/native/sort_contract.cpp)
target_link_libraries(point_zero_native_sort PUBLIC point_zero_native_catalog point_zero_native_memory
    point_zero_native_state)
target_include_directories(point_zero_native_sort PUBLIC include src)
target_compile_definitions(point_zero_native_sort PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
point_zero_warnings(point_zero_native_sort)

add_library(point_zero_native_filter STATIC src/native/original_filter_state.cpp src/native/original_filter.cpp
    src/native/filter_contract.cpp)
target_link_libraries(point_zero_native_filter PUBLIC point_zero_native_catalog point_zero_native_memory
    point_zero_native_state)
target_include_directories(point_zero_native_filter PUBLIC include src)
target_compile_definitions(point_zero_native_filter PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
point_zero_warnings(point_zero_native_filter)

add_library(point_zero_native_summary_entry OBJECT src/native/category_summary_entry.asm)
add_library(point_zero_native_level STATIC src/native/level_population.cpp src/native/level_membership.cpp
    src/native/category_storage.cpp src/native/category_storage_contract.cpp
    src/native/category_metadata.cpp src/native/category_metadata_contract.cpp
    src/native/category_summary.cpp src/native/category_summary_contract.cpp
    src/native/category_accessors.cpp src/native/category_accessors_contract.cpp
    src/native/category_badges.cpp src/native/category_badges_contract.cpp
    src/native/category_bank.cpp src/native/category_generations.cpp
    src/native/category_roots.cpp src/native/category_vector_contract.cpp
    src/native/category_lifecycle_hooks.cpp src/native/category_lifecycle_contract.cpp
    src/native/category_navigation.cpp src/native/category_navigation_contract.cpp src/native/category_chart_match.cpp
    src/native/category_requests.cpp src/native/category_request_contract.cpp src/native/category_request_sites_contract.cpp
    src/native/category_restore_contract.cpp
    $<TARGET_OBJECTS:point_zero_native_summary_entry>)
target_link_libraries(point_zero_native_level PUBLIC point_zero_native_catalog point_zero_native_memory point_zero_native_state
    point_zero_native_filter
    PRIVATE point_zero_native_hook safetyhook::safetyhook)
target_include_directories(point_zero_native_level PUBLIC include src)
target_compile_features(point_zero_native_level PRIVATE cxx_std_23)
target_compile_definitions(point_zero_native_level PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
point_zero_warnings(point_zero_native_level)

add_library(point_zero_native_browsing STATIC src/native/browsing.cpp)
target_link_libraries(point_zero_native_browsing PUBLIC point_zero_native_level
    point_zero_native_sort point_zero_native_filter point_zero_native_hook)
target_compile_definitions(point_zero_native_browsing PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
point_zero_warnings(point_zero_native_browsing)

add_library(point_zero_native_folders STATIC src/native/folder_definitions.cpp
    src/native/folder_art.cpp src/native/folder_contract.cpp src/native/folder_art_contract.cpp)
target_include_directories(point_zero_native_folders PUBLIC include src)
target_link_libraries(point_zero_native_folders PUBLIC point_zero_native_catalog
    point_zero_native_memory point_zero_native_state)
target_compile_definitions(point_zero_native_folders PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
point_zero_warnings(point_zero_native_folders)

add_library(point_zero_native_adapters STATIC src/native/adapters.cpp src/native/guide.cpp)
target_link_libraries(point_zero_native_adapters PUBLIC point_zero_native_surfaces
    point_zero_native_browsing point_zero_native_folders PRIVATE safetyhook::safetyhook)
target_compile_features(point_zero_native_adapters PRIVATE cxx_std_23)
target_compile_definitions(point_zero_native_adapters PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
point_zero_warnings(point_zero_native_adapters)

# The actual core links the selected game driver and complete native composition.
# Building it is not admission/qualification or permission to install it. Unlike
# preparation_contract below, no test driver or native-success fixture is linked.
add_library(point_zero_core SHARED src/core/api.cpp "${POINT_ZERO_NOTICES_RESOURCE}")
target_include_directories(point_zero_core PRIVATE include src)
target_include_directories(point_zero_core SYSTEM PRIVATE "${POINT_ZERO_RAPIDJSON_INCLUDE_DIR}")
target_link_libraries(point_zero_core PRIVATE point_zero_native_driver point_zero_native_adapters)
target_compile_definitions(point_zero_core PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
point_zero_warnings(point_zero_core)


add_library(point_zero_inputs STATIC src/loader/input_snapshot.cpp)
target_include_directories(point_zero_inputs PUBLIC include src)
target_compile_definitions(point_zero_inputs PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
point_zero_warnings(point_zero_inputs)

add_library(point_zero_release STATIC src/loader/release.cpp)
target_include_directories(point_zero_release PUBLIC src)
target_include_directories(point_zero_release SYSTEM PRIVATE "${POINT_ZERO_RAPIDJSON_INCLUDE_DIR}")
target_link_libraries(point_zero_release PRIVATE bcrypt)
point_zero_warnings(point_zero_release)

add_library(point_zero_transaction STATIC src/loader/files.cpp src/loader/transaction.cpp src/loader/game_major_win.cpp)
target_compile_definitions(point_zero_transaction PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(point_zero_transaction SYSTEM PRIVATE "${POINT_ZERO_RAPIDJSON_INCLUDE_DIR}")
target_link_libraries(point_zero_transaction PUBLIC point_zero_release point_zero_inputs PRIVATE crypt32)
point_zero_warnings(point_zero_transaction)

add_library(point_zero_http STATIC src/loader/http.cpp)
target_include_directories(point_zero_http PUBLIC src)
target_compile_definitions(point_zero_http PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(point_zero_http PRIVATE winhttp shlwapi)
point_zero_warnings(point_zero_http)

add_library(point_zero_startup STATIC src/loader/startup.cpp)
target_include_directories(point_zero_startup PUBLIC include src)
target_compile_definitions(point_zero_startup PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(point_zero_startup PUBLIC point_zero_transaction)
point_zero_warnings(point_zero_startup)

add_library(point_zero_staging STATIC src/loader/staging.cpp)
target_link_libraries(point_zero_staging PUBLIC point_zero_transaction)
point_zero_warnings(point_zero_staging)

add_library(point_zero_zip_reader STATIC ${miniz_SOURCE_DIR}/miniz.c
    ${miniz_SOURCE_DIR}/miniz_tinfl.c ${miniz_SOURCE_DIR}/miniz_zip.c)
target_include_directories(point_zero_zip_reader SYSTEM PUBLIC "${miniz_SOURCE_DIR}" "${miniz_BINARY_DIR}")
target_compile_definitions(point_zero_zip_reader PUBLIC MINIZ_NO_ARCHIVE_WRITING_APIS
    MINIZ_NO_DEFLATE_APIS MINIZ_NO_ZLIB_APIS MINIZ_NO_ZLIB_COMPATIBLE_NAMES
    MINIZ_NO_STDIO MINIZ_NO_TIME MINIZ_STATIC_DEFINE)
add_library(point_zero_package STATIC src/loader/zip.cpp src/loader/github.cpp src/loader/game_major.cpp)
target_include_directories(point_zero_package SYSTEM PRIVATE "${POINT_ZERO_RAPIDJSON_INCLUDE_DIR}")
target_link_libraries(point_zero_package PUBLIC point_zero_release PRIVATE point_zero_zip_reader)
point_zero_warnings(point_zero_package)

set(POINT_ZERO_SPICE_SDK_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/_deps/spice-sdk/include")
set(POINT_ZERO_UPDATE_REPOSITORY "cyl0/point-zero-updates" CACHE STRING "Public GitHub owner/repository hosting update ZIPs")
if(POINT_ZERO_UPDATE_REPOSITORY)
    if(NOT POINT_ZERO_UPDATE_REPOSITORY MATCHES "^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$")
        message(FATAL_ERROR "Expected a GitHub owner/repository")
    endif()
    configure_file(src/loader/release_settings.cpp.in generated/release_settings.cpp @ONLY)
    add_library(point_zero_loader SHARED src/loader/sdk_entry.cpp "${POINT_ZERO_NOTICES_RESOURCE}")
    target_include_directories(point_zero_loader PRIVATE "${POINT_ZERO_SPICE_SDK_INCLUDE_DIR}")
    target_compile_definitions(point_zero_loader PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    point_zero_warnings(point_zero_loader)
    add_library(point_zero_updater SHARED src/loader/updater_entry.cpp "${CMAKE_CURRENT_BINARY_DIR}/generated/release_settings.cpp"
        "${POINT_ZERO_NOTICES_RESOURCE}")
    target_include_directories(point_zero_updater PRIVATE src "${POINT_ZERO_SPICE_SDK_INCLUDE_DIR}")
    target_compile_definitions(point_zero_updater PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(point_zero_updater PRIVATE point_zero_startup point_zero_staging point_zero_http point_zero_package)
    point_zero_warnings(point_zero_updater)
else()
    message(STATUS "Production loader packaging disabled until the GitHub repository is supplied")
endif()

