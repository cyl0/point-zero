#pragma once
#include <string>

namespace point_zero::loader {
struct ReleaseSettings {
    std::string repository;
};
// GitHub owner/repository fixed at build time, not writable game configuration.
// Test loader targets provide an explicitly test-only definition.
ReleaseSettings release_settings();
} // namespace point_zero::loader
