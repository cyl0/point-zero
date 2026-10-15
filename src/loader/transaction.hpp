#pragma once

#include "files.hpp"
#include "input_snapshot.hpp"
#include "release.hpp"

#include <functional>

namespace point_zero::loader {
struct VerifiedGeneration {
    Release release;
    std::filesystem::path directory;
    // Manifest first. Candidates then retain every artifact in release order;
    // installed generations retain only the consumed updater/core/art payloads.
    std::vector<std::unique_ptr<FilePin>> pins;
};

// One synchronous owner of journal/activation/ACK. The caller performs downloads
// and core preparation between begin/offer/activate, and retains InputSnapshot
// through terminal admission. No rating semantics or game hooks live here.
class Transaction {
public:
    using Event = std::function<void(std::string_view)>;
    Transaction(const std::filesystem::path& runtime, Event event = {},
                const std::atomic_bool* cancelled = nullptr);
    ~Transaction();
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    // Recover first, then verify/pin the installed manifest and updater/core/art. Rating
    // inputs are protected separately by InputSnapshot. Also usable to
    // recover a rejected pre-native attempt after its input snapshot is released.
    // Never call after native initialization began; the loader enforces that.
    void begin();
    bool offer(std::string_view generation_id, Channel requested, std::uint32_t game_major = 0);
    const VerifiedGeneration& selected() const;
    InputSnapshot::Replacements replacements() const;
    void discard_offer(); // only before activation/native initialization
    void activate(InputSnapshot& prepared_inputs);
    void finish(bool native_admitted, bool cancelled = false); // false retains recovery journal

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace point_zero::loader
