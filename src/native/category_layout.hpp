#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace point_zero::native::category_layout {
// Reviewed LDJ2026081900 storage, copied as values. Addresses are never C++
// containers; their native allocation, lifetime and mutators remain separate.
struct Vector {
    std::uintptr_t begin{}, end{}, capacity{};
    bool operator==(const Vector&) const = default;
};
struct Bar {
    std::int32_t type{};
    std::uint32_t flags{};
    std::uintptr_t data{};
    bool operator==(const Bar&) const = default;
};
struct Chart {
    std::uintptr_t music{};
    std::int32_t p1{}, p2{}, original_p1{}, original_p2{}, style{}, category{};
    std::array<std::int32_t, 5> bar_style{};
    std::array<std::byte, 12> tail{};
    bool operator==(const Chart&) const = default;
};
struct String {
    std::array<char, 16> storage{};
    std::size_t size{}, capacity{};
    bool operator==(const String&) const = default;
};
struct Category {
    std::int32_t id{};
    std::uint8_t open{};
    std::array<std::byte, 3> padding{};
    std::int32_t bar_count{}, chart_count{}, insert_count{}, cursor{};
    std::array<std::int32_t, 2> preferences{};
    Vector bars;
    std::int32_t header_id{};
    std::array<std::int32_t, 2> header_preferences{};
    std::array<std::byte, 4> header_padding{};
    String label;
    std::int32_t random_id{};
    std::array<std::int32_t, 2> random_preferences{};
    std::array<std::byte, 4> random_padding{};
    Vector charts;
    std::uintptr_t auxiliary_vtable{};
    std::array<std::byte, 32> auxiliary_fields{};
    Vector auxiliary;
    bool operator==(const Category&) const = default;
};
static_assert(sizeof(Vector) == 24 && sizeof(Bar) == 16 && sizeof(Chart) == 64 && sizeof(String) == 32);
static_assert(std::is_standard_layout_v<Category> && std::is_trivially_copyable_v<Category>);
static_assert(offsetof(Chart, p1) == 8 && offsetof(Chart, style) == 0x18 &&
              offsetof(Chart, original_p1) == 0x10 && offsetof(Chart, original_p2) == 0x14 &&
              offsetof(Chart, category) == 0x1C);
static_assert(sizeof(Category) == 0xD0 && offsetof(Category, bars) == 0x20 &&
              offsetof(Category, charts) == 0x78 && offsetof(Category, cursor) == 0x14);
static_assert(offsetof(Category, preferences) == 0x18 && offsetof(Category, header_id) == 0x38 &&
              offsetof(Category, header_preferences) == 0x3C && offsetof(Category, label) == 0x48 &&
              offsetof(Category, random_id) == 0x68 && offsetof(Category, random_preferences) == 0x6C &&
              offsetof(Category, auxiliary_vtable) == 0x90 && offsetof(Category, auxiliary) == 0xB8);
} // namespace point_zero::native::category_layout
