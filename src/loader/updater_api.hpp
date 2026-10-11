#pragma once

#include "updater_protocol.hpp"
#include <spicesdk.h>

// The permanent bootstrap knows only this C ABI and the local generation ID.
// No manifests, network policy, core ABI or C++ ownership cross this boundary.
struct UpdaterRequest {
    const wchar_t* runtime;
    const SPICE_SDK_V0* sdk;
    const char* offer; // Empty on an ordinary launch or installed fallback.
    uint32_t skip_update;
    int (__cdecl *stopping)();
};
struct UpdaterReply {
    char generation[65];
    uint32_t offered;
};
struct UpdaterApi {
    uint32_t abi;
    int (__cdecl *run)(const UpdaterRequest*, UpdaterReply*) noexcept;
    void (__cdecl *stop)() noexcept;
};
using GetUpdaterApi = const UpdaterApi* (__cdecl *)(uint32_t) noexcept;
