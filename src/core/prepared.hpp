#pragma once

#include "point_zero/core_api.h"
#include "ratings/ratings.hpp"

#include <memory>

// This definition never crosses the C ABI. Creation and destruction stay in
// the core's CRT, and the native owner retains it through native retirement.
struct PZ_Prepared {
    std::string generation_id;
    std::string release_version; // Cosmetic metadata from the pinned release manifest.
    point_zero::ratings::Snapshot ratings;
    std::string cache; // Optional replacement; a valid input cache needs no rewrite.
};

namespace point_zero::core {
struct NativeRequest {
    std::filesystem::path runtime_path, generation_path;
    void* completion_context;
    PZ_Completion completion;
    PZ_NativeHost host; // Value snapshot; callback/context lifetime ends after shutdown.
};

// Implemented by the native integration target. It must take durable ownership
// before publishing hooks; it catches errors and completes admission once.
int32_t initialize_native(std::unique_ptr<PZ_Prepared>, NativeRequest, PZ_Diagnostic) noexcept;
void shutdown_native() noexcept;
} // namespace point_zero::core
