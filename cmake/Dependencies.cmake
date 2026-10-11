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
    URL "https://codeload.github.com/Tencent/rapidjson/tar.gz/refs/tags/v1.1.0"
    URL_HASH SHA256=bf7ced29704a1e696fbccf2a2b4ea068e7774fa37f6d7dd4039d0787f8bed98e
    SOURCE_SUBDIR headers-only
    EXCLUDE_FROM_ALL)
FetchContent_Declare(miniz
    URL "https://codeload.github.com/richgel999/miniz/tar.gz/174573d60290f447c13a2b1b3405de2b96e27d6c"
    URL_HASH SHA256=3cb61250e4ec1890423273fe58752f1e63d9a6d0c3b58dea61e6fe6c82d6d349
    SOURCE_SUBDIR headers-only
    EXCLUDE_FROM_ALL)
FetchContent_Declare(zycore
    URL "https://codeload.github.com/zyantific/zycore-c/tar.gz/refs/tags/v1.5.2"
    URL_HASH SHA256=943f91eb9ab2a8cc01ab9f8b785e769a273502071e0ee8011cdfcaad93947cec
    SOURCE_SUBDIR headers-only
    EXCLUDE_FROM_ALL)
FetchContent_Declare(Zydis
    URL "https://codeload.github.com/zyantific/zydis/tar.gz/refs/tags/v4.1.1"
    URL_HASH SHA256=45c6d4d499a1cc80780f7834747c637509777c01dca1e98c5e7c0bfaccdb1514
    EXCLUDE_FROM_ALL)
FetchContent_Declare(safetyhook
    URL "https://codeload.github.com/cursey/safetyhook/tar.gz/refs/tags/v0.7.0"
    URL_HASH SHA256=72a738986c73f7b78bf1eb41680e8aacf5df5664fe23c8d96d7c018f170282ab
    EXCLUDE_FROM_ALL)

FetchContent_MakeAvailable(yaml_cpp rapidjson miniz zycore)
set(ZYAN_ZYCORE_PATH "${zycore_SOURCE_DIR}" CACHE PATH "" FORCE)
FetchContent_MakeAvailable(safetyhook)
file(MAKE_DIRECTORY "${miniz_BINARY_DIR}")
file(WRITE "${miniz_BINARY_DIR}/miniz_export.h" "#pragma once\n#define MINIZ_EXPORT\n")

# Fetch only the SDK interface header; no Spice2X implementation is built.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/_deps/spice-sdk/include")
file(DOWNLOAD "https://raw.githubusercontent.com/spice2x/spice2x.github.io/531e7a54b30a85ae4c7884734fd574641b30b6e0/src/spice2x/sdk/include/spicesdk.h"
    "${CMAKE_CURRENT_BINARY_DIR}/_deps/spice-sdk/include/spicesdk.h"
    EXPECTED_HASH SHA256=667b28667301b202f76295eb4e5ffa9937e81fe35100dd2ec9c5e266c511db83 TLS_VERIFY ON)
