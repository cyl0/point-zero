#pragma once

#include "level_population.hpp"

#include <memory>
#include <string_view>

namespace point_zero::native {
struct CategoryDefinition {
    std::uint32_t flags{};
    std::int32_t default_sort{};
    std::uint8_t difficulty_switchable{}, difficulty_locked{};
    std::array<std::byte, 6> padding{};
    const char *hover_header{}, *hover_subheader{}, *bar_text{}, *bar_background{}, *ticker{};
    std::int32_t capacity{};
    std::array<std::byte, 4> tail{};
};
static_assert(sizeof(CategoryDefinition) == 64 && offsetof(CategoryDefinition, capacity) == 0x38);

// Complete code contracts and metadata-hook routing are caller prerequisites.
// Storage uses only native construction/allocation/assignment/insertion/free.
struct CategoryNativeCalls {
    void (__fastcall* construct)(std::uintptr_t){};
    std::intptr_t (__fastcall* initialize)(std::uintptr_t, int){};
    std::uintptr_t (__fastcall* assign_string)(std::uintptr_t, const char*, std::size_t){};
    std::uint8_t (__fastcall* insert)(const void* const*, int, int, int){};
    void (__fastcall* destroy)(std::uintptr_t){};
    std::uintptr_t auxiliary_vtable{};
};

enum class CategoryStorageState { empty, live, destroyed, uncertain };

// One native category's backing storage, not a published hierarchy. The caller
// owns scene/group references, mode/policy and publication. Keep definition art
// strings and native code alive; serialize operations on the preparation thread.
class CategoryStorage final {
public:
    CategoryStorage(MemoryView, CategoryNativeCalls);
    ~CategoryStorage() noexcept; // No native calls. Retains backing if not destroyed.
    CategoryStorage(const CategoryStorage&) = delete;
    CategoryStorage& operator=(const CategoryStorage&) = delete;
    // One build attempt. Takes the already validated current eligible member set.
    // capacity is set from members, avoiding native vector growth per insertion.
    // Keep the member span and label immutable for the entire call.
    // Failure after uncertain native effects is terminal and preserves backing.
    bool build(std::uintptr_t manager, int id, int style, CategoryDefinition,
               std::string_view label, std::span<const LevelChart>) noexcept;
    // Call only after proving there are NO native consumers, including delayed
    // group references. This component cannot establish that external fact.
    // Never from core shutdown merely because a feature gate is closed.
    bool destroy() noexcept;
    CategoryStorageState state() const noexcept;
    bool complete() const noexcept;
    // Read-only allocation/guard validation at an owned native boundary. This
    // queries protection; do not use it in drawing or lookup callbacks.
    bool intact() const noexcept;
    // Cheap check for admitted operations that retain category storage.
    // Does not grant allocation ownership or authorize native destruction.
    bool descriptors() const noexcept;
    std::uintptr_t address() const noexcept;
    const CategoryDefinition* definition() const noexcept;
    // The admitted metadata hook consults this only during the native build.
    // Unknown manager/ID returns null so the hook forwards its native original.
    static const CategoryDefinition* active_definition(std::uintptr_t manager, int id) noexcept;
private:
    struct Backing;
    std::unique_ptr<Backing> backing_;
    bool attempted_{}, busy_{};
    std::uint32_t thread_{};
    bool build_native(std::uintptr_t, int, int, CategoryDefinition,
                      std::string_view, std::span<const LevelChart>);
    bool build_checked(std::uintptr_t, int, int, CategoryDefinition,
                       std::string_view, std::span<const LevelChart>) noexcept;
    bool destroy_native();
    bool destroy_checked() noexcept;
};
} // namespace point_zero::native
