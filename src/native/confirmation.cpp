#include "confirmation.hpp"
#include "confirmation_metrics.hpp"

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace point_zero::native {
namespace {
constexpr char letters[] = "bnhal";
constexpr const char* children[]{"whole", "fraction", "point"};
constexpr const char* leaves[]{"decide_diff_num_1p", "decide_diff_num_2p"};
}

ConfirmationRenderer::ConfirmationRenderer(const AdapterContext& context, CupAfp afp,
                                             ConfirmationNativeCalls calls)
    : memory_(context.memory), catalog_(context.catalog), state_(context.state),
      host_(context.host), afp_(afp), calls_(calls) {
    if (!afp.valid || !afp.path || !afp.relative || !afp.package || !afp.animation_info ||
        !afp.attach || !afp.control || !afp.path_relative || !afp.property ||
        !calls.style || !calls.logical_count || !calls.sole_player || !calls.raw_mode ||
        !calls.published_music || !calls.published_difficulties || !calls.name_table ||
        !calls.setter_callers[0] || !calls.setter_callers[1] || !calls.builder_caller ||
        !calls.named_callers[0] || !calls.named_callers[1])
        throw std::invalid_argument("incomplete admitted confirmation calls");
}

bool ConfirmationRenderer::same_thread() const noexcept {
    return thread_ && thread_ == GetCurrentThreadId();
}
bool ConfirmationRenderer::live_package() const noexcept {
    int state{};
    std::uint32_t id{};
    return same_thread() && read_field(memory_, slot_, 0x510, state) && state == 2 &&
        read_field(memory_, slot_, 0x514, id) && id == package_ && afp_.package(family.data()) == id;
}
bool ConfirmationRenderer::preload_impl(std::uintptr_t slot) {
    if (attempted_ || state_.view().phase != FeaturePhase::preparing) return false;
    attempted_ = true;
    thread_ = GetCurrentThreadId(); slot_ = slot;
    std::array<char, 256> name{};
    if (!read_field(memory_, slot, 8, name) ||
        std::string_view(name.data(), family.size()) != family || name[family.size()] ||
        !read_field(memory_, slot, 0x514, package_) || (package_ & 0x78000000U) != 0x30000000U ||
        !live_package()) return false;
    const auto load = [&](const char* resource, std::uint32_t& out) {
        std::uint64_t info[8]{};
        if (afp_.animation_info(info, package_, resource) ||
            (std::uint32_t(info[3]) & 0x70000000U) != 0x30000000U) return false;
        out = std::uint32_t(info[3]);
        return true;
    };
    if (!load("lab_glyph_bs", component_)) return false;
    for (unsigned f = 0; f < bank_.size(); ++f) {
        auto& bank = bank_[f];
        for (unsigned part = 1; part < 24; ++part) {
            char resource[40]{};
            std::uint32_t* out{};
            if (part < 13) {
                std::snprintf(resource, sizeof(resource), "pz34_c%c_w%02u", letters[f], part);
                out = &bank.whole[part];
            } else if (part < 23) {
                std::snprintf(resource, sizeof(resource), "pz34_c%c_f%u", letters[f], part - 13);
                out = &bank.fraction[part - 13];
            } else {
                std::snprintf(resource, sizeof(resource), "pz34_c%c_point", letters[f]);
                out = &bank.point;
            }
            if (!load(resource, *out)) return false;
        }
    }
    prepared_ = live_package();
    return prepared_;
}
bool ConfirmationRenderer::preload(std::uintptr_t slot) noexcept {
    __try { return preload_impl(slot); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void ConfirmationRenderer::fault() noexcept {
    if (warned_.test_and_set(std::memory_order_relaxed) || !host_.log) return;
    constexpr std::string_view message = "confirmation drawing skipped a native operation; later callbacks remain enabled";
    try { host_.log(host_.context, PZ_LOG_WARNING,
        {reinterpret_cast<const std::uint8_t*>(message.data()), message.size()}); } catch (...) {}
}

bool ConfirmationRenderer::read_binding(std::uintptr_t owner, Binding& out) const noexcept {
    out.owner = owner;
    return owner >= 0x10000 && read_field(memory_, owner, 0x28, out.music) && out.music >= 0x10000 &&
        read_field(memory_, owner, 0x30, out.style) && out.style >= 0 && out.style <= 1 &&
        read_field(memory_, out.music, 0x67C, out.song) && out.song > 0 &&
        read_field(memory_, owner, 0x34, out.difficulties) && read_field(memory_, owner, 0x3C, out.levels);
}
void ConfirmationRenderer::begin_set() noexcept {
    if (same_thread()) binding_ = {};
}
void ConfirmationRenderer::bind(std::uintptr_t owner, std::uintptr_t music, std::uintptr_t caller) noexcept {
    if (!same_thread()) return;
    __try {
        binding_ = {};
        Binding candidate;
        if ((caller == calls_.setter_callers[0] || caller == calls_.setter_callers[1]) &&
            read_binding(owner, candidate) && candidate.music == music && candidate.style == calls_.style())
            binding_ = candidate;
    } __except (EXCEPTION_EXECUTE_HANDLER) { binding_ = {}; fault(); }
}
void ConfirmationRenderer::begin_impl(std::uintptr_t owner, std::uintptr_t caller) {
    if (!same_thread()) { fault(); return; }
    if (++depth_ != 1) return;
    current_ = {};
    if (caller != calls_.builder_caller || state_.view().phase != FeaturePhase::ready ||
        !state_.view().enabled() || !prepared_ || !live_package() || binding_.owner != owner) return;
    Binding observed;
    if (!read_binding(owner, observed) || observed != binding_ || observed.style != calls_.style() ||
        (calls_.raw_mode() != 0 && observed.style == 0)) return;
    std::uintptr_t music{}, layer{};
    std::array<int, 2> difficulties{};
    if (!read_value(memory_, calls_.published_music, music) || music != observed.music ||
        !read_value(memory_, calls_.published_difficulties, difficulties) || difficulties != observed.difficulties ||
        !read_field(memory_, owner, 8, layer) || layer < 0x10000 ||
        !read_field(memory_, layer, 8, current_.stream) || afp_.valid(5, current_.stream) < 0) return;
    current_.count = calls_.logical_count();
    const int sole = calls_.sole_player();
    if ((current_.count != 1 && current_.count != 2) || sole < 0 || sole > 1) return;
    // Native first copy already mirrors the sole physical player's chart;
    // logical count gates the second copy, including ordinary DP.
    current_.binding = observed;
    current_.valid = true;
}
void ConfirmationRenderer::begin(std::uintptr_t owner, std::uintptr_t caller) noexcept {
    __try { begin_impl(owner, caller); }
    __except (EXCEPTION_EXECUTE_HANDLER) { current_.valid = false; fault(); }
}
void ConfirmationRenderer::end() noexcept {
    if (same_thread() && depth_ && --depth_ == 0) current_ = {};
}
void ConfirmationRenderer::begin_named_impl(std::uintptr_t wrapper, const char* name, std::uintptr_t caller) {
    if (depth_ != 1 || !current_.valid || !name || !state_.view().enabled()) return;
    const int copy = caller == calls_.named_callers[0] ? 0 : (caller == calls_.named_callers[1] ? 1 : -1);
    if (copy < 0 || copy >= current_.count) return;
    const auto& binding = current_.binding;
    const int difficulty = binding.difficulties[copy], level = binding.levels[copy];
    if (difficulty < 0 || difficulty > 4 || level < 1 || level > 12) return;
    std::int8_t raw{};
    if (!read_field(memory_, binding.music, 0x3EC + 5ULL * binding.style + difficulty, raw) || raw != level) return;
    const auto row = catalog_.find({binding.song, binding.style, difficulty});
    if (!row || !row->assigned || row->native_level != level ||
        row->effective_tenths < 10 || row->effective_tenths > 129) return;
    // Beginner/Leggendaria native table entries alias several levels. Identity
    // comes from the validated tuple, never from parsing the texture name.
    std::uintptr_t expected{};
    std::array<char, 32> expected_name{};
    if (!read_field(memory_, calls_.name_table, 8ULL * (difficulty * 13 + level), expected) ||
        !read_field(memory_, expected, 0, expected_name) || !std::memchr(expected_name.data(), 0, expected_name.size()) ||
        std::strcmp(expected_name.data(), name)) return;
    int wrapped{};
    if (!read_field(memory_, wrapper, 0x200, wrapped) || wrapped < 0) return;
    int handle = afp_.path(current_.stream, leaves[copy]);
    if (handle < 0 || handle != wrapped) return;
    while (handle >= 0 && named_.count < named_.handles.size()) {
        if (afp_.valid(6, handle) < 0) return;
        named_.handles[named_.count++] = handle;
        handle = afp_.relative(handle, 6);
    }
    if (handle >= 0 || !named_.count) return;
    named_.chart = row;
    named_.valid = true;
}
void ConfirmationRenderer::begin_named(std::uintptr_t wrapper, const char* name, std::uintptr_t caller) noexcept {
    if (!same_thread() || ++named_depth_ != 1) return;
    named_ = {};
    __try { begin_named_impl(wrapper, name, caller); }
    __except (EXCEPTION_EXECUTE_HANDLER) { named_.valid = false; fault(); }
}
void ConfirmationRenderer::end_named() noexcept {
    if (same_thread() && named_depth_ && --named_depth_ == 0) named_ = {};
}

void ConfirmationRenderer::collect(Owner& owner) {
    if (!owner.mc) return;
    if (afp_.valid(5, owner.stream) < 0 || afp_.valid(6, owner.mc) < 0) { owner = {}; return; }
    if (!owner.restoring) return;
    for (int child : owner.children) if (child > 0 && afp_.valid(6, child) >= 0) return;
    for (int ink : owner.inks) if (ink > 0 && afp_.valid(6, ink) >= 0) return;
    owner = {};
}
bool ConfirmationRenderer::restore(Owner& owner, int mc, const char* name, int mode,
                                    int& result, StockBitmap stock) {
    result = stock(mc, name, mode);
    if (!result) owner.restoring = true;
    else fault();
    return true;
}
bool ConfirmationRenderer::apply(Owner* owner, int mc, const char* name, int mode,
                                  int& result, StockBitmap stock) {
    if (!owner) {
        for (auto& candidate : owners_) if (!candidate.mc) { owner = &candidate; break; }
        if (!owner) {
            for (auto& candidate : owners_) collect(candidate);
            for (auto& candidate : owners_) if (!candidate.mc) { owner = &candidate; break; }
        }
        if (!owner) return false;
    }
    const auto row = named_.chart;
    const int value = row->effective_tenths, palette = row->key.difficulty;
    const auto& bank = bank_[palette];
    const auto& placement = confirmation_art::families[palette];
    const bool fresh = !owner->mc;
    const bool changed = fresh || owner->value != value || owner->palette != palette;
    int rc{};
    if (fresh) {
        *owner = {}; owner->owner = current_.binding.owner; owner->stream = current_.stream; owner->mc = mc;
        rc = afp_.attach(mc, component_);
        if (!rc) rc = afp_.control(mc, 0xF04, 0);
    }
    const std::uint32_t wanted[]{bank.whole[value / 10], bank.fraction[value % 10], bank.point};
    for (int i = 0; i < 3 && !rc; ++i) {
        const int child = afp_.path_relative(mc, children[i]);
        if (child <= 0 || afp_.valid(6, child) < 0 ||
            (owner->children[i] && owner->children[i] != child)) { rc = -1; break; }
        owner->children[i] = child;
        if (owner->parts[i] != wanted[i]) {
            owner->inks[i] = 0;
            rc = afp_.attach(child, wanted[i]);
            if (!rc) rc = afp_.control(child, 0xF04, 0);
            owner->parts[i] = wanted[i];
        }
        if (!rc && changed) {
            const int ink = owner->inks[i] ? owner->inks[i] : afp_.path_relative(child, "ink");
            if (ink <= 0 || afp_.valid(6, ink) < 0) rc = -1;
            else owner->inks[i] = ink;
        }
        if (!rc && changed) {
            const float scale[]{placement.scale, placement.scale};
            const float position[]{placement.x[value][i], placement.y};
            rc = afp_.property(child, 0x1003, reinterpret_cast<std::intptr_t>(scale));
            if (!rc) rc = afp_.property(child, 0x1000, reinterpret_cast<std::intptr_t>(position));
            if (!rc) rc = afp_.property(child, 0x101E, 0);
        }
    }
    if (rc || !state_.view().enabled()) {
        if (rc) fault();
        return restore(*owner, mc, name, mode, result, stock);
    }
    owner->value = value; owner->palette = palette;
    result = 0;
    return true;
}
bool ConfirmationRenderer::bitmap_impl(int mc, const char* name, int mode, int& result, StockBitmap stock) {
    if (!same_thread() || !stock || mc <= 0) return false;
    Owner* owned{};
    for (auto& candidate : owners_) if (candidate.mc == mc) {
        if (state_.view().phase == FeaturePhase::ready) collect(candidate);
        if (candidate.mc) owned = &candidate;
        break;
    }
    const auto forward = [&] { return owned ? restore(*owned, mc, name, mode, result, stock) : false; };
    if (!state_.view().enabled() || !current_.valid || !named_.valid || depth_ != 1 || named_depth_ != 1 ||
        !name || mode != 0 || !live_package()) return forward();
    bool found{};
    for (unsigned i = 0; i < named_.count; ++i) found |= named_.handles[i] == mc;
    if (!found || afp_.valid(5, current_.stream) < 0 || afp_.valid(6, mc) < 0 ||
        (owned && (owned->stream != current_.stream || owned->owner != current_.binding.owner || owned->restoring)))
        return forward();
    return apply(owned, mc, name, mode, result, stock);
}
bool ConfirmationRenderer::bitmap(int mc, const char* name, int mode, int& result, StockBitmap stock) noexcept {
    __try { return bitmap_impl(mc, name, mode, result, stock); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        fault(); result = -1;
        // An attachment or stock restoration may already have run; never retry.
        return true;
    }
}
} // namespace point_zero::native
