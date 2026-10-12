include(FetchContent)
set(CMAKE_TLS_VERIFY ON)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SAFETYHOOK_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(SAFETYHOOK_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(SAFETYHOOK_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_DOXYGEN OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZYCORE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZYCORE_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(yaml_cpp
    URL "https://codeload.github.com/jbeder/yaml-cpp/tar.gz/c5e65360373d1d99d6b6848c991cd4fcee435009"
    URL_HASH SHA256=aa33e48f7ea186fa8ccbc3eb9c5a85b831f3f56cf0920764ba390eb090ae7160
    EXCLUDE_FROM_ALL)
FetchContent_Declare(rapidjson
    URL "https://codeload.github.com/Tencent/rapidjson/tar.gz/24b5e7a8b27f42fa16b96fc70aade9106cf7102f"
    URL_HASH SHA256=2d2601a82d2d3b7e143a3c8d43ef616671391034bc46891a9816b79cf2d3e7a8
    SOURCE_SUBDIR headers-only
    EXCLUDE_FROM_ALL)
FetchContent_Declare(miniz
    URL "https://codeload.github.com/richgel999/miniz/tar.gz/77d0dce8627735138c51770d1799a1ef48f2117d"
    URL_HASH SHA256=baa14349e7c5efb4a493a03af7d2f29d0ef45d87d488567fe5360edb33f8b7ba
    SOURCE_SUBDIR headers-only
    EXCLUDE_FROM_ALL)
FetchContent_Declare(zycore
    URL "https://codeload.github.com/zyantific/zycore-c/tar.gz/0b2432ced0884fd152b471d97ecf0258ff4d859f"
    URL_HASH SHA256=698e4623e661db312eb9da327519f53f0a9dd7fbbfacf02159687bca62369467
    SOURCE_SUBDIR headers-only
    EXCLUDE_FROM_ALL)
FetchContent_Declare(Zydis
    URL "https://codeload.github.com/zyantific/zydis/tar.gz/a2278f1d254e492f6a6b39f6cb5d1f5d515659dc"
    URL_HASH SHA256=efbca210596ef954241ef40732376d50be5b6b7ca7d63a3834ef0c71f3471f36
    EXCLUDE_FROM_ALL)
FetchContent_Declare(safetyhook
    URL "https://codeload.github.com/cursey/safetyhook/tar.gz/f44cc070a8340f2f26649553c49533475417304d"
    URL_HASH SHA256=97c37b0c99ab5f3e8ba517946765ef628addc84f3be93ec6dc1661f2621fff18
    SOURCE_SUBDIR headers-only
    EXCLUDE_FROM_ALL)

FetchContent_MakeAvailable(yaml_cpp rapidjson miniz zycore)
set(ZYAN_ZYCORE_PATH "${zycore_SOURCE_DIR}" CACHE PATH "" FORCE)
FetchContent_MakeAvailable(Zydis safetyhook)
include("${safetyhook_SOURCE_DIR}/cmake/CPM.cmake")
CPMRegisterPackage(zydis 4.1.1)
add_subdirectory("${safetyhook_SOURCE_DIR}" "${safetyhook_BINARY_DIR}" EXCLUDE_FROM_ALL)
file(MAKE_DIRECTORY "${miniz_BINARY_DIR}")
file(WRITE "${miniz_BINARY_DIR}/miniz_export.h" "#pragma once\n#define MINIZ_EXPORT\n")

# Fetch only the SDK interface header; no Spice2X implementation is built.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/_deps/spice-sdk/include")
file(DOWNLOAD "https://raw.githubusercontent.com/spice2x/spice2x.github.io/fdc7adff7f0560923633c5438a4be7d2f55a0f38/src/spice2x/sdk/include/spicesdk.h"
    "${CMAKE_CURRENT_BINARY_DIR}/_deps/spice-sdk/include/spicesdk.h"
    EXPECTED_HASH SHA256=1e292d9ece962f91d1020ff79381eada06ccd5736a9f0f69496f25dfab2d2109 TLS_VERIFY ON)
