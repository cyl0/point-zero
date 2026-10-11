#include "cup.hpp"

#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace point_zero::native {
namespace {
constexpr char letters[] = "bnhal";
constexpr const char* children[] = {"whole", "fraction", "point"};
constexpr auto layer_vtable = 0xDC86E0;

int animated(int target, int counter, int duration) noexcept {
    const float progress = float(std::clamp(counter, 0, duration)) / float(duration);
    return progress >= 1.f ? target : int((std::min)(1.f, ::sinf(progress * 1.5707964f)) * float(target));
}
template<class Owner> void collect_owner(Owner& owner, const CupAfp& afp) {
    if (!owner.mc) return;
    if (afp.valid(5, owner.stream) < 0 || afp.valid(6, owner.mc) < 0) { owner = {}; return; }
    if (owner.restoring) {
        bool retired = true;
        for (int child : owner.children) if (child > 0 && afp.valid(6, child) >= 0) retired = false;
        for (int ink : owner.inks) if (ink > 0 && afp.valid(6, ink) >= 0) retired = false;
        if (retired) owner = {};
    }
}
} // namespace

CupRenderer::CupRenderer(const AdapterContext& context, CupAfp afp)
    : game_(context.modules.game.range), memory_(context.memory), catalog_(context.catalog),
      state_(context.state), host_(context.host), afp_(afp) {
    if (!afp.valid || !afp.path || !afp.relative || !afp.package ||
        !afp.animation_info || !afp.attach || !afp.control || !afp.path_relative || !afp.property)
        throw std::invalid_argument("incomplete admitted cup AFP calls");
}

bool CupRenderer::same_thread() const noexcept { return thread_ && thread_ == GetCurrentThreadId(); }
bool CupRenderer::live_package() const noexcept {
    int state{};
    std::uint32_t id{};
    return same_thread() && read_field(memory_, slot_, 0x510, state) && state == 2 &&
        read_field(memory_, slot_, 0x514, id) && id == package_ && afp_.package(family.data()) == id;
}
bool CupRenderer::preload_impl(std::uintptr_t slot) {
    if (attempted_ || state_.view().phase != FeaturePhase::preparing) return false;
    attempted_ = true;
    thread_ = GetCurrentThreadId(); slot_ = slot;
    std::array<char, 256> name{};
    if (!read_field(memory_, slot, 8, name) ||
        std::string_view(name.data(), family.size()) != family || name[family.size()] != '\0' ||
        !read_field(memory_, slot, 0x514, package_) || (package_ & 0x78000000U) != 0x30000000U ||
        !live_package()) return false;
    for (unsigned f = 0; f < bank_.size(); ++f) {
        const char letter = f < 5 ? letters[f] : 'b', variant = f < 5 ? 's' : 'c';
        auto& bank = bank_[f];
        for (unsigned part = 0; part < 25; ++part) {
            char resource[40]{};
            std::uint32_t* out{};
            if (part == 0) {
                std::snprintf(resource, sizeof(resource), "lab_glyph_%c%c", letter, variant); out = &bank.container;
            } else if (part <= 13) {
                std::snprintf(resource, sizeof(resource), "lab_glyph_%c%c_p%03u", letter, variant, part);
                out = &bank.whole[part - 1];
            } else if (part <= 23) {
                std::snprintf(resource, sizeof(resource), "pz_cf_%c%c_%u", letter, variant, part - 14);
                out = &bank.fraction[part - 14];
            } else {
                std::snprintf(resource, sizeof(resource), "pz_cp_%c%c", letter, variant); out = &bank.point;
            }
            std::uint64_t info[8]{};
            if (afp_.animation_info(info, package_, resource) ||
                (std::uint32_t(info[3]) & 0x70000000U) != 0x30000000U) return false;
            *out = std::uint32_t(info[3]);
        }
    }
    prepared_ = live_package();
    return prepared_;
}
bool CupRenderer::preload(std::uintptr_t slot) noexcept {
    __try { return preload_impl(slot); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void CupRenderer::fault() noexcept {
    if (warned_.test_and_set(std::memory_order_relaxed) || !host_.log) return;
    constexpr std::string_view message = "cup drawing skipped a native operation; later callbacks remain enabled";
    try { host_.log(host_.context, PZ_LOG_WARNING,
        {reinterpret_cast<const std::uint8_t*>(message.data()), message.size()}); } catch (...) {}
}
CupRenderer::Owner* CupRenderer::find(int mc) noexcept {
    for (auto& owner : owners_) if (owner.mc == mc) return &owner;
    return nullptr;
}
void CupRenderer::collect() {
    for (auto& owner : owners_) collect_owner(owner, afp_);
}
void CupRenderer::begin_impl(std::uintptr_t owner) {
    if (!same_thread()) { fault(); return; }
    if (++depth_ != 1) return;
    current_ = {}; current_.owner = owner;
    const auto view = state_.view();
    if (view.phase != FeaturePhase::ready) return;
    if (!view.enabled()) return;
    if (!prepared_ || !live_package()) { fault(); return; }
    std::uintptr_t clock{};
    int mode{}, rate{};
    if (!read_field(memory_, owner, 0x34, current_.counter) || !read_field(memory_, game_.base, 0xABAF550, clock) ||
        !read_field(memory_, clock, 0x260, mode) || !read_field(memory_, clock, 0x270, rate)) return;
    if (mode == 2) rate = 60;
    if (rate < 1) return;
    current_.duration = int(float(rate) * 0.26666999f + .5f);
    current_.enabled = current_.duration > 0 &&
        read_cup_values(memory_, game_, catalog_, owner, current_.values);
}
void CupRenderer::begin(std::uintptr_t owner) noexcept {
    __try { begin_impl(owner); }
    __except (EXCEPTION_EXECUTE_HANDLER) { fault(); }
}
void CupRenderer::end() noexcept {
    if (same_thread() && depth_ && --depth_ == 0) current_ = {};
}

bool CupRenderer::restore(Owner& owner, int mc, const char* name, int mode,
                          int& result, StockBitmap stock) {
    result = stock(mc, name, mode);
    if (result == 0) owner.restoring = true;
    else fault();
    return true;
}
bool CupRenderer::apply(Owner* owner, int mc, std::uint32_t stream, int family_index, int value,
                        const char* name, int mode, int& result, StockBitmap stock) {
    if (!owner) {
        for (auto& candidate : owners_) if (!candidate.mc) { owner = &candidate; break; }
        if (!owner) {
            // Untouched leases need polling only when the fixed table is full.
            collect();
            for (auto& candidate : owners_) if (!candidate.mc) { owner = &candidate; break; }
        }
        // No attachment or stock call has begun for this unowned bitmap.
        if (!owner) return false;
    }
    const auto& bank = bank_[family_index];
    const bool fresh = !owner->mc, replaced = fresh || owner->component != bank.container;
    const bool changed = replaced || owner->value != value;
    cup_art::Placement placement;
    if (!cup_art::placement(family_index, value, placement)) { fault(); return false; }
    int rc{};
    if (fresh) { *owner = {}; owner->cup = current_.owner; owner->stream = stream; owner->mc = mc; }
    if (replaced) {
        rc = afp_.attach(mc, bank.container);
        if (!rc) rc = afp_.control(mc, 0xF04, 0);
        owner->component = bank.container;
        std::fill(std::begin(owner->children), std::end(owner->children), 0);
        std::fill(std::begin(owner->parts), std::end(owner->parts), 0);
        std::fill(std::begin(owner->inks), std::end(owner->inks), 0);
    }
    const std::uint32_t wanted[]{bank.whole[value / 10], bank.fraction[value % 10], bank.point};
    for (int i = 0; i < 3 && !rc; ++i) {
        // Native can replace a child while the old full handle is still alive.
        // Keep the parent association check; validity alone cannot authorize
        // writes to that retained child. Ink lookup is cached below.
        const int child = afp_.path_relative(mc, children[i]);
        if (child <= 0 || afp_.valid(6, child) < 0 ||
            (!replaced && owner->children[i] && child != owner->children[i])) { rc = -1; break; }
        owner->children[i] = child;
        if (owner->parts[i] != wanted[i]) {
            owner->inks[i] = 0; // The old ink belongs to the replaced part.
            rc = afp_.attach(child, wanted[i]);
            if (!rc) rc = afp_.control(child, 0xF04, 0);
            owner->parts[i] = wanted[i];
        }
        // Ink is passive. Check it during presentation changes and retirement,
        // while settled frames still verify the child association and pose.
        if (!rc && changed) {
            const int ink = owner->inks[i] ? owner->inks[i] : afp_.path_relative(child, "ink");
            if (ink <= 0 || afp_.valid(6, ink) < 0) rc = -1;
            else owner->inks[i] = ink;
        }
        const auto& position = placement[i];
        if (!rc && changed) {
            rc = afp_.property(child, 0x1003, reinterpret_cast<std::intptr_t>(position.scale.data()));
            if (!rc) rc = afp_.property(child, 0x1000, reinterpret_cast<std::intptr_t>(position.position.data()));
            if (!rc) rc = afp_.property(child, 0x101E, 0);
        }
    }
    if (rc) {
        fault();
        // Restore only through the game's original binding. No guessed detach,
        // deletion, package release or repair of a foreign local transform.
        return restore(*owner, mc, name, mode, result, stock);
    }
    if (!state_.view().enabled()) return restore(*owner, mc, name, mode, result, stock);
    owner->value = value;
    result = 0;
    return true;
}

bool CupRenderer::bitmap_impl(int mc, const char* name, int mode, int& result, StockBitmap stock) {
    if (!same_thread() || !stock || mc <= 0) return false;
    auto* owned = find(mc);
    if (owned && state_.view().phase == FeaturePhase::ready) {
        // Reclaim the touched full handle before restoration or custom reuse.
        collect_owner(*owned, afp_);
        if (!owned->mc) owned = nullptr;
    }
    const auto forward = [&] { return owned ? restore(*owned, mc, name, mode, result, stock) : false; };
    if (!state_.view().enabled() || !current_.enabled || depth_ != 1 || !name || mode != 0) return forward();
    const auto length = std::strlen(name);
    if ((length != 5 && length != 9) || name[0] != 'n' || name[1] != '_' ||
        name[3] < '0' || name[3] > '1' || name[4] < '0' || name[4] > '9') return forward();
    const auto letter = std::strchr(letters, name[2]);
    if (!letter || !*letter || (length == 9 && std::strcmp(name + 5, "_sel"))) return forward();
    const int difficulty = int(letter - letters), native_value = (name[3] - '0') * 10 + name[4] - '0';
    if (native_value < 1 || native_value > 12) return forward();

    std::uintptr_t layer{}, vt{};
    std::uint32_t stream{};
    if (!read_field(memory_, current_.owner, 8ULL * difficulty, layer) || !read_field(memory_, layer, 0, vt) ||
        vt != game_.base + layer_vtable || !read_field(memory_, layer, 8, stream) || afp_.valid(5, stream) < 0) return forward();
    char leaf[]{'d','i','f','f','_',letters[difficulty],0};
    bool found{};
    int handle = afp_.path(stream, leaf);
    unsigned count{};
    for (; handle >= 0 && count < 8; ++count, handle = afp_.relative(handle, 6)) {
        if (afp_.valid(6, handle) < 0) break;
        found |= handle == mc;
    }
    if (!found || count >= 8) return forward();
    if (owned && (owned->stream != stream || owned->cup != current_.owner)) { fault(); return forward(); }
    if (owned && owned->restoring) return forward();
    const auto chart = current_.values.charts[difficulty];
    if (!chart || !chart->assigned ||
        std::clamp(animated(chart->native_level, current_.counter, current_.duration), 1, 12) != native_value) return forward();
    const int value = animated(chart->effective_tenths, current_.counter, current_.duration);
    return apply(owned, mc, stream, length == 9 ? difficulty : 5, value, name, mode, result, stock);
}
bool CupRenderer::bitmap(int mc, const char* name, int mode, int& result, StockBitmap stock) noexcept {
    __try { return bitmap_impl(mc, name, mode, result, stock); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        fault(); result = -1;
        // The fault may follow a stock call or attachment; never invoke it twice.
        return true;
    }
}
} // namespace point_zero::native
