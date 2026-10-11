#pragma once

#include "adapters.hpp"
#include "played_calls.hpp"

#include <array>
#include <atomic>

namespace point_zero::native {
class ResultPlate;

// Original paired musicinfo_2pp NOTES owners. No native allocation, detach or
// release: the adapter retains this object for as long as callbacks can enter.
class ResultNotes final {
public:
    ResultNotes(const AdapterContext&, PlayedNativeCalls, ResultPlate&);
    ~ResultNotes() = default;
    ResultNotes(const ResultNotes&) = delete;
    ResultNotes& operator=(const ResultNotes&) = delete;
    ResultNotes(ResultNotes&&) = delete;
    ResultNotes& operator=(ResultNotes&&) = delete;

    // One attempt, while preparing, on the native callback thread. The parent
    // supplies complete admitted calls and keeps all operations on that thread.
    bool prepare() noexcept;
    // The renderer supplies its admitted observation from this capture. Later
    // NOTES updates still resolve the saved tuple afresh before using it.
    void remember(const PlayedCapture&, const PlayedChart&) noexcept;
    void update(std::uintptr_t wrapper) noexcept;

private:
    struct Capture {
        PlayedCapture input{};
        PlayedChart chart{};
    };
    struct Node {
        std::uintptr_t work{};
        int handle{-1};
        float offset{};
    };
    struct Owner {
        std::uintptr_t wrapper{}, root_work{}, source{};
        std::uint32_t stream{};
        int root{-1};
        std::array<Node, 6> nodes{};
    };

    ImageRange game_, afp_;
    MemoryView memory_;
    const Catalog& catalog_;
    FeatureState& state_;
    PZ_NativeHost host_;
    PlayedNativeCalls calls_;
    ResultPlate& plate_;
    std::array<Capture, 2> captures_{};
    // Four overlapping native wrapper generations, six original roles each.
    // Exhaustion skips this callback; possibly touched work is not evicted.
    std::array<Owner, 4> owners_{};
    bool attempted_{}, prepared_{};
    std::atomic_bool logged_{}; // Diagnostic deduplication, never an enable gate.

    void fault() noexcept;
    void remember_impl(const PlayedCapture&, const PlayedChart&);
    void update_impl(std::uintptr_t);
    void collect_impl();
    bool alive(const Owner&);
    bool matches(const Owner&, const Node&, const played_art::NotesRole&);
    bool preflight(Owner&);
    bool move(Owner&, std::size_t, float);
    std::array<float, 2> wanted();
};
} // namespace point_zero::native
