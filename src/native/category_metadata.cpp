#include "category_metadata.hpp"
#include "category_accessors.hpp"
#include "category_bank.hpp"
#include "inline_patch.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <stdexcept>
#include <string>

namespace point_zero::native {
namespace {
void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
struct OwnedDefinition {
    int id{};
    CategoryDefinition value;
    std::array<std::string, 5> text;
};
std::array<const char**, 5> text_fields(CategoryDefinition& d) noexcept {
    return {&d.hover_header, &d.hover_subheader, &d.bar_text, &d.bar_background, &d.ticker};
}
std::vector<OwnedDefinition> copy_definitions(std::span<const CategoryMetadataEntry> input) {
    require(!input.empty() && input.size() <= 144, "private category definitions missing or excessive");
    std::vector<OwnedDefinition> result(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        const auto& source = input[i];
        require(source.id >= 230 && (!i || input[i - 1].id < source.id) &&
                source.definition.difficulty_locked == 1, "private category definition ID/order/lock invalid");
        auto& out = result[i];
        out.id = source.id; out.value = source.definition; out.value.capacity = 0;
        const auto fields = text_fields(out.value);
        for (std::size_t f = 0; f < fields.size(); ++f) {
            if (!*fields[f]) continue; // Native null artwork selects its existing string branch.
            const auto length = strnlen_s(*fields[f], 256);
            require(length < 256, "private category metadata string too long");
            out.text[f].assign(*fields[f], length);
            *fields[f] = out.text[f].c_str();
        }
    }
    return result; // Vector storage transfers intact; short-string addresses stay stable.
}
} // namespace

struct CategoryMetadata::State {
    using Original = const CategoryDefinition* (__fastcall*)(std::uintptr_t, int);
    enum class Phase { disabled, failed, armed };
    static std::atomic<bool> reserved;
    static std::atomic<State*> live;
    const std::vector<OwnedDefinition> definitions;
    detail::InlinePatch patch;
    const Original original;
    Phase phase{Phase::disabled}; // Preparation/intact/destruction are externally serialized.
    bool retained{};
    State(MemoryView m, const CategoryMetadataSite& site, std::span<const CategoryMetadataEntry> entries)
        : definitions(copy_definitions(entries)),
          patch(m, site.address, site.body, reinterpret_cast<std::uintptr_t>(&dispatch)),
          original(reinterpret_cast<Original>(patch.original())) {}
    const CategoryDefinition* find(int id) const noexcept {
        if (id < definitions.front().id || id > definitions.back().id) return nullptr;
        const auto at = std::lower_bound(definitions.begin(), definitions.end(), id,
            [](const OwnedDefinition& entry, int wanted) { return entry.id < wanted; });
        return at != definitions.end() && at->id == id ? &at->value : nullptr;
    }
    static const CategoryDefinition* __fastcall dispatch(std::uintptr_t manager, int id) {
        auto& self = *live.load(std::memory_order_acquire);
        if (const auto* constructing = CategoryStorage::active_definition(manager, id)) return constructing;
        if (const auto* constructing = CategoryBank::active_definition(manager, id)) return constructing;
        const int index = CategoryAccessors::definition_index(manager, id);
        if (index >= 0 && static_cast<std::size_t>(index) < self.definitions.size()) return &self.definitions[index].value;
        return self.original(manager, id);
    }
    bool intact() const noexcept { return phase == Phase::armed && patch.intact(); }
};
std::atomic<bool> CategoryMetadata::State::reserved{};
std::atomic<CategoryMetadata::State*> CategoryMetadata::State::live{};

CategoryMetadata::CategoryMetadata(MemoryView memory, const CategoryMetadataSite& site,
                                   std::span<const CategoryMetadataEntry> entries) {
    bool available{};
    require(State::reserved.compare_exchange_strong(available, true), "category metadata process slot already reserved");
    try {
        require(memory.read && memory.query && site.address >= 0x10000 && !site.body.empty() &&
                site.body.size() <= UINTPTR_MAX - site.address, "category metadata site missing or invalid");
        state_ = new State(memory, site, entries);
    } catch (...) { State::reserved.store(false, std::memory_order_release); throw; }
}
CategoryMetadata::~CategoryMetadata() noexcept {
    // Definitions are native return values, not callbacks into a wrapper/bank.
    // They and their strings must outlive delayed native consumers. Forwarding
    // also uses only retained state; it never reads the original MemoryView.
    if (!state_->retained) { delete state_; State::reserved.store(false, std::memory_order_release); }
}
void CategoryMetadata::arm() {
    auto& self = *state_;
    require(self.phase == State::Phase::disabled, "category metadata arm already attempted");
    self.phase = State::Phase::failed;
    require(self.patch.before_intact(), "category metadata changed before arm");
    self.retained = true;
    State::live.store(&self, std::memory_order_release);
    self.patch.enable();
    require(self.patch.intact(), "category metadata changed during arm");
    self.phase = State::Phase::armed;
}
bool CategoryMetadata::intact() const noexcept { return state_->intact(); }
const CategoryDefinition* CategoryMetadata::definition(int id) const noexcept { return state_->find(id); }
} // namespace point_zero::native
