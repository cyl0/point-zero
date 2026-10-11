#pragma once

#include "catalog.hpp"
#include "memory.hpp"
#include "category_layout.hpp"

#include <array>
#include <span>
#include <vector>

namespace point_zero::native {
// An existing chart in the current native LEVEL population, not merely a
// registered chart. Original difficulty supplies identity; working difficulty
// remains native and can differ after VEFX. No scores or unlocks are cached.
struct LevelChart {
    std::uintptr_t music{};
    ratings::Key key;
    int native_level{}, effective_tenths{};
    bool assigned{};
    bool operator==(const LevelChart&) const = default;
};

class LevelPopulation final {
public:
    static constexpr std::size_t style_limit = 20000;
    // Catalogue is immutable for this owner's lifetime. Capture resolves it
    // once per capture, with freshly validated manager/list ownership.
    LevelPopulation(MemoryView, ImageRange game, const Catalog&);
    LevelPopulation(const LevelPopulation&) = delete;
    LevelPopulation& operator=(const LevelPopulation&) = delete;
    void prepare(); // One attempt; allocates scratch and pins the native thread.
    // Only after the caller's admitted ordinary native population rebuild and
    // list adoption. The caller owns mode/BATTLE, publication and failure policy.
    // Read both disjoint integer banks. Never call eligibility/get-or-create,
    // infer availability from registration, or read the overlapping L folder.
    bool capture(std::uintptr_t manager, std::uintptr_t list) noexcept;
    bool valid() const noexcept { return valid_; }
    std::span<const LevelChart> charts() const noexcept;
    // Borrowed spans are invalidated by the next admitted capture, even one
    // that fails. Wrong-thread/reentrant calls never alter an owner's snapshot.

private:
    struct Source {
        int id{}, count{}, inserted{};
        category_layout::Vector charts;
    };
    MemoryView memory_;
    ImageRange game_;
    const Catalog& catalog_;
    std::vector<LevelChart> charts_;
    std::uintptr_t manager_{}, list_{};
    std::size_t count_{};
    std::uint32_t thread_{};
    bool attempted_{}, prepared_{}, busy_{}, valid_{};
    bool owner() const noexcept;
    bool source(unsigned, Source&) const noexcept;
    bool collect() noexcept;
};
} // namespace point_zero::native
