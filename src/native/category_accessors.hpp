#pragma once

#include "memory.hpp"
#include <array>
#include <vector>

namespace point_zero::native {
enum class CategoryAccessor : unsigned { lookup, active, clear, rival, kac, recommend, shop, weekly, count };
struct CategoryAccessorSite { std::uintptr_t address{}; std::vector<std::byte> body; };
struct CategoryAccessorSites {
    std::array<CategoryAccessorSite, static_cast<unsigned>(CategoryAccessor::count)> entries;
    CategoryAccessorSite style; // Native selector resolver; admitted, never patched.
};
struct CategoryAccessBinding {
    int id{}, style{};
    std::uintptr_t category{}, summary{};
};

class CategoryAccessors final {
public:
    CategoryAccessors(MemoryView, const CategoryAccessorSites&, std::span<const int> ids);
    ~CategoryAccessors() noexcept;
    CategoryAccessors(const CategoryAccessors&) = delete;
    CategoryAccessors& operator=(const CategoryAccessors&) = delete;
    void arm(); // One attempt on the native browsing thread; mapping starts empty.
    // Two sorted ID sets, style 0 then 1. Empty bindings forward to native.
    bool bind(std::uintptr_t manager, std::span<const CategoryAccessBinding>) noexcept;
    bool intact() const noexcept;
    static int definition_index(std::uintptr_t manager, int id) noexcept;
private:
    struct State;
    State* state_{};
};
} // namespace point_zero::native
