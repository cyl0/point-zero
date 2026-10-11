#pragma once

#include "adapters.hpp"
#include "played_calls.hpp"

namespace point_zero::native {
class ResultPlate final {
public:
    ResultPlate(const AdapterContext&, PlayedNativeCalls);
    ~ResultPlate() = default; // Native owners, children and packages outlive this object.
    ResultPlate(const ResultPlate&) = delete;
    ResultPlate& operator=(const ResultPlate&) = delete;
    ResultPlate(ResultPlate&&) = delete;
    ResultPlate& operator=(ResultPlate&&) = delete;

    // Complete calls and same native thread are supplied by PlayedRenderer.
    bool prepare(std::uint32_t component) noexcept;
    // false forbids publishing a custom numeral which needs this extension.
    bool apply(const PlayedTarget&, const played_art::Plan&, bool custom, PlayedStockBitmap) noexcept;
    void early(const PlayedChart&, const played_art::Plan&, bool custom, PlayedStockBitmap) noexcept;
    // Settle verified retired attachments and report only acknowledged extent.
    float extension(const PlayedChart&) noexcept;

private:
    struct Node { int handle{-1}; std::uintptr_t work{}; };
    struct Owner {
        // Borrowed chart/leaf pointers are compared only, never dereferenced.
        PlayedChart chart;
        played_art::ResultPlate layout;
        Node root, plate, number;
        std::array<Node, 3> children{};
        std::uintptr_t source{}, definition{};
        bool name_touched{}, cleared{}, clear_attempted{}, exposed{};
        bool replaced{}, pending{}, children_known{};
        float extension{};
    };
    struct Life { bool gone{}, children_gone{}; };
    enum class Location { absent, admitted, rejected };

    ImageRange game_, afp_;
    MemoryView memory_;
    FeatureState& state_;
    PZ_NativeHost host_;
    PlayedNativeCalls calls_;
    std::array<Owner, 8> owners_{};
    std::uint32_t component_{};
    bool attempted_{}, prepared_{}, busy_{};

    std::atomic_flag warned_{};
    void fault() noexcept;
    bool enter() noexcept;
    bool chart_live(const PlayedChart&) const;
    static bool same_chart(const PlayedChart&, const PlayedChart&) noexcept;
    bool node(const Node&, std::uint32_t, std::uintptr_t parent, bool root = false,
              bool exposed = true) const;
    bool owner_valid(const Owner&) const;
    bool original(const Owner&) const;
    bool name_is(const Owner&, bool named) const;
    bool plate_at_depth(std::uintptr_t root, int depth, std::uintptr_t& plate, unsigned& matches) const;
    bool siblings(const Owner&, const PlayedTarget*, bool early) const;
    Location locate(const PlayedTarget&, played_art::ResultPlate, bool, Owner&) const;
    bool children(Owner&, bool discover);
    bool preflight(std::array<Life, 8>&, const PlayedTarget* = nullptr, bool early = true) const;
    bool collect_impl(const PlayedTarget* = nullptr, bool early = true, std::array<Life, 8>* observed = nullptr);
    bool clear(Owner&);
    bool restore(Owner&, PlayedStockBitmap);
    bool apply_impl(const PlayedTarget&, const played_art::Plan&, bool, PlayedStockBitmap, bool early);
    float extension_impl(const PlayedChart&);
};
} // namespace point_zero::native
