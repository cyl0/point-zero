#pragma once

#include "adapters.hpp"
#include "assistant_header.hpp"

namespace point_zero::native {
class AssistantRenderer final {
public:
    static constexpr std::string_view family = "/pz34_assistant.ifs";
    using StockBitmap = int (__fastcall*)(int, const char*, int);
    // Borrow the shared SurfaceHooks original, which must outlive our hook drain.
    AssistantRenderer(const AdapterContext&, StockBitmap);
    ~AssistantRenderer() noexcept;
    AssistantRenderer(const AssistantRenderer&) = delete;
    AssistantRenderer& operator=(const AssistantRenderer&) = delete;
    // One attempt on the native preparation thread, before feature publication.
    bool preload(std::uintptr_t slot) noexcept;
    // Pass this renderer as the hook's opaque context; drain before destruction.
    static AssistantHeaderHook::Callbacks callbacks() noexcept;

private:
    struct State;
    std::unique_ptr<State> state_;
};
} // namespace point_zero::native
