#pragma once

#include "point_zero/core_api.h"

#include <atomic>
#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

namespace point_zero::loader {

// Loader-owned bytes and deny-write handles. This knows no rating semantics.
// Replacement targets must be declared by the verified release and either
// registered or absent. Logical paths stay installed paths when bytes are staged;
// absent targets become registered only when their transaction activates.
class InputSnapshot {
public:
    using Replacements = std::vector<std::pair<std::filesystem::path, std::filesystem::path>>;
    explicit InputSnapshot(Replacements = {}, const std::atomic_bool* cancelled = nullptr);
    ~InputSnapshot();
    InputSnapshot(const InputSnapshot&) = delete;
    InputSnapshot& operator=(const InputSnapshot&) = delete;

    PZ_Bytes read(const std::filesystem::path& logical);

    // After managed activation, capture every installed dependency and compare
    // exact bytes. Keep old protection until every new handle is verified. No
    // second parse or fit; any disagreement rejects activation before native init.
    void bind_installed();

    static int32_t PZ_CALL read_for_core(void*, PZ_Bytes logical, PZ_Bytes* out,
                                       PZ_Diagnostic) noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace point_zero::loader
