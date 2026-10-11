#include "assets.hpp"

#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace point_zero::native {
namespace {
bool native_load(void*, std::uintptr_t entry, std::uintptr_t manager,
                 const char* prefix, const char* name, std::uintptr_t& result) noexcept {
    result = 0;
    if (!entry) return false;
    using Load = std::uintptr_t(__fastcall*)(void*, const char*, const char*, int, unsigned char);
    __try {
        result = reinterpret_cast<Load>(entry)(reinterpret_cast<void*>(manager), prefix, name, 0, 0);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool native_release(void*, std::uintptr_t entry, std::uintptr_t slot) noexcept {
    if (!entry) return false;
    using Release = std::intptr_t(__fastcall*)(std::uintptr_t, unsigned char);
    __try { (void)reinterpret_cast<Release>(entry)(slot, 0); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
std::uint32_t native_thread(void*) noexcept { return GetCurrentThreadId(); }

bool path(std::string_view value) noexcept {
    // Generated private package paths only, not user rating/input path policy.
    if (value.size() < 2 || value.size() >= 256 || value.front() != '/' || value.back() == '/') return false;
    for (const auto ch : value) {
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '/' || ch == '_' || ch == '-' || ch == '.')) return false;
    }
    for (std::size_t begin = 1; begin < value.size();) {
        auto end = value.find('/', begin);
        if (end == std::string_view::npos) end = value.size();
        const auto segment = value.substr(begin, end - begin);
        if (segment.empty() || segment == "." || segment == "..") return false;
        begin = end + 1;
    }
    return true;
}
bool string_value(const std::array<char, 256>& bytes, std::string_view& out) noexcept {
    const auto end = std::find(bytes.begin(), bytes.end(), '\0');
    if (end == bytes.end()) return false;
    out = {bytes.data(), static_cast<std::size_t>(end - bytes.begin())};
    return true;
}
bool matches(const std::array<char, 256>& bytes, std::string_view value) noexcept {
    std::string_view actual;
    return string_value(bytes, actual) && actual == value;
}
bool bank_id(std::uint32_t id) noexcept { return (id & 0x78000000U) == 0x30000000U; }
} // namespace

AssetCalls guarded_asset_calls() noexcept { return {nullptr, native_load, native_release, native_thread}; }

// This gate is held across every callback, including reads and thread lookup.
// Nested calls cannot change transaction state or enter a second native call.
class AssetOwner::Operation {
public:
    explicit Operation(AssetOwner& owner) noexcept : owner_(owner) {
        acquired_ = !owner_.busy_.test_and_set(std::memory_order_acquire);
        if (!acquired_) { error = AssetError::busy; return; }
        const auto thread = owner_.calls_.thread_id(owner_.calls_.context);
        if (!thread || (owner_.thread_ && owner_.thread_ != thread)) {
            error = AssetError::wrong_thread;
            return;
        }
        owner_.thread_ = thread;
    }
    ~Operation() { if (acquired_) owner_.busy_.clear(std::memory_order_release); }
    Operation(const Operation&) = delete;
    Operation& operator=(const Operation&) = delete;
    AssetError error = AssetError::none;
private:
    AssetOwner& owner_;
    bool acquired_{};
};

AssetOwner::AssetOwner(std::string private_prefix, std::span<const std::string_view> families,
                       AssetNative native, MemoryView memory, AssetCalls calls)
    : prefix_(std::move(private_prefix)), native_(native), memory_(memory), calls_(calls) {
    if (!path(prefix_) || !prefix_.ends_with("/assets") || prefix_ == "/assets" ||
        families.empty() || families.size() > slot_count || !native_.load || !native_.release ||
        native_.manager < 0x10000 ||
        !contains(native_.manager, slot_count * slot_stride, native_.manager, slot_count * slot_stride) ||
        !memory_.read || !memory_.query || !calls_.load || !calls_.release || !calls_.thread_id)
        throw std::invalid_argument("invalid private asset owner configuration");
    families_.reserve(families.size());
    for (const auto name : families) {
        // The game formats "/afp%d%s" into 256 bytes (index 0..511), but
        // AVS fs_mount parses that mount path in a smaller 128-byte buffer.
        if (!path(name) || prefix_.size() + name.size() + 7 >= 128 ||
            std::any_of(families_.begin(), families_.end(), [name](const Family& f) { return f.name == name; }))
            throw std::invalid_argument("invalid or duplicate private asset family");
        families_.push_back({std::string(name), prefix_ + std::string(name), false});
    }
    leases_.reserve(families.size());
    before_.resize(slot_count);
    after_.resize(slot_count);
}

AssetOwner::Identity AssetOwner::identity(const SlotBytes& bytes) noexcept {
    Identity result;
    std::memcpy(result.name.data(), bytes.data() + 8, result.name.size());
    std::memcpy(result.source.data(), bytes.data() + 0x108, result.source.size());
    std::memcpy(&result.refs, bytes.data() + 0x50C, sizeof(result.refs));
    std::memcpy(&result.state, bytes.data() + 0x510, sizeof(result.state));
    std::memcpy(&result.id, bytes.data() + 0x514, sizeof(result.id));
    return result;
}
bool AssetOwner::vacant(const Identity& slot) noexcept {
    // AFP read failure returns -1 (missing) or -8 (invalid header). Native
    // release clears the free slot except for that non-bank error ID.
    const bool read_error = slot.id == UINT32_MAX || slot.id == UINT32_MAX - 7;
    const bool empty = slot.name == decltype(slot.name){} && slot.source == decltype(slot.source){};
    return slot.refs == 0 && slot.state == 0 && (slot.id == 0 || (read_error && empty));
}
bool AssetOwner::same_resource(const Identity& a, const Identity& b) noexcept {
    return a.name == b.name && a.source == b.source && a.id == b.id;
}
bool AssetOwner::capture(std::vector<SlotBytes>& slots) const noexcept {
    return read_bytes(memory_, native_.manager, std::as_writable_bytes(std::span(slots)));
}
bool AssetOwner::read_slot(std::uintptr_t address, Identity& out) const noexcept {
    SlotBytes bytes;
    if (!read_bytes(memory_, address, bytes)) return false;
    out = identity(bytes);
    return true;
}
AssetResult AssetOwner::fail(AssetError error) noexcept {
    state_ = AssetState::failed;
    return {error, 0};
}
std::size_t AssetOwner::owned_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(leases_.begin(), leases_.end(),
        [](const Lease& lease) { return !lease.released; }));
}

AssetResult AssetOwner::load(std::string_view name) noexcept {
    Operation operation(*this);
    if (operation.error != AssetError::none) return {operation.error};
    if (state_ != AssetState::collecting) return {AssetError::invalid_state};
    const auto family = std::find_if(families_.begin(), families_.end(),
        [name](const Family& f) { return f.name == name; });
    if (family == families_.end()) return fail(AssetError::unknown_family);
    if (family->loaded) return fail(AssetError::duplicate_family);
    if (!capture(before_)) return fail(AssetError::unreadable);

    // Never let the native lookup increment a preexisting resource's refs. A
    // collision in either field is sufficient; both fields must be terminated.
    bool available = false;
    for (const auto& bytes : before_) {
        const auto slot = identity(bytes);
        std::string_view slot_name, source;
        if (!string_value(slot.name, slot_name) || !string_value(slot.source, source)) continue;
        if (slot_name == family->name || source == family->source) return fail(AssetError::collision);
        // Native name lookup visits even state-zero slots, and its allocator
        // considers state alone. Refuse stale collisions or nonempty "free" slots.
        if (slot.state == 0) available = true;
    }
    if (!available) return fail(AssetError::load_failed);

    std::uintptr_t returned{};
    const bool called = calls_.load(calls_.context, native_.load, native_.manager,
                                    prefix_.c_str(), family->name.c_str(), returned);
    // A successful loader return identifies its slot. Changes to other slots,
    // padding or unrelated resource references do not invalidate this load.
    if (called && returned >= native_.manager &&
        returned - native_.manager < slot_count * slot_stride &&
        (returned - native_.manager) % slot_stride == 0) {
        Identity slot;
        if (vacant(identity(before_[(returned - native_.manager) / slot_stride])) &&
            read_slot(returned, slot) && matches(slot.name, family->name) &&
            matches(slot.source, family->source) && slot.state == 2 && slot.refs > 0 && bank_id(slot.id)) {
            leases_.push_back({returned, slot, false, false, false});
            family->loaded = true;
            return {AssetError::none, returned};
        }
    }
    if (!called) uncertain_ = true;
    if (!capture(after_)) {
        uncertain_ = true;
        return fail(called ? AssetError::unreadable : AssetError::native_fault);
    }
    std::size_t allocations = 0, changed = 0, candidate = 0;
    for (std::size_t i = 0; i < slot_count; ++i) {
        const auto before = identity(before_[i]), after = identity(after_[i]);
        // An ordinary failed read can finish native cleanup with only a known
        // error ID left behind. This proves no lease, not an uncertain effect.
        const bool cleaned_error = called && !returned && vacant(before) && vacant(after) &&
            after.id != 0 && before.name == decltype(before.name){} &&
            before.source == decltype(before.source){};
        if (before_[i] != after_[i] && !cleaned_error) ++changed;
        if (vacant(before) && !vacant(after)) { ++allocations; candidate = i; }
    }
    if (!allocations && !changed && !returned)
        return fail(called ? AssetError::load_failed : AssetError::native_fault);
    if (allocations != 1 || changed != 1) {
        uncertain_ = true;
        return fail(called ? AssetError::ambiguous_allocation : AssetError::native_fault);
    }
    const auto slot = identity(after_[candidate]);
    if (!matches(slot.name, family->name) || !matches(slot.source, family->source) ||
        (slot.id != 0 && !bank_id(slot.id)) || slot.state < 1 || slot.state > 2 || slot.refs < 0 || slot.refs > 1) {
        uncertain_ = true;
        return fail(called ? AssetError::identity_changed : AssetError::native_fault);
    }
    // A new slot must not alias a native package already present before loading.
    if (slot.id && std::any_of(before_.begin(), before_.end(), [&slot](const SlotBytes& bytes) {
            return identity(bytes).id == slot.id;
        })) {
        uncertain_ = true;
        return fail(called ? AssetError::ambiguous_allocation : AssetError::native_fault);
    }
    // A unique formerly vacant slot and exact source/name prove this lease even
    // when a failed native load has not yet assigned a package ID.
    const auto address = native_.manager + candidate * slot_stride;
    leases_.push_back({address, slot, false, false, false}); // Capacity reserved before calls.
    if (returned && returned != address) {
        uncertain_ = true;
        return fail(called ? AssetError::ambiguous_allocation : AssetError::native_fault);
    }
    if (!called) return fail(AssetError::native_fault);
    if (returned != address || slot.state != 2 || slot.refs != 1 || !bank_id(slot.id))
        return fail(AssetError::load_failed);
    family->loaded = true;
    return {AssetError::none, address};
}

AssetResult AssetOwner::rollback() noexcept {
    Operation operation(*this);
    if (operation.error != AssetError::none) return {operation.error};
    if (state_ == AssetState::published) return {AssetError::invalid_state};
    if (state_ == AssetState::rolled_back) return {};
    const auto incomplete = [this](AssetError error) {
        state_ = AssetState::rollback_incomplete;
        return AssetResult{error};
    };
    // No release at all if a known bank is retained by consumers. This also
    // preserves reverse order across retries after a pre-call read/ref failure.
    for (auto it = leases_.rbegin(); it != leases_.rend(); ++it) {
        if (it->released) continue;
        // An observed reuse cannot be undone by matching bytes later (ABA).
        if (it->identity_lost) return incomplete(AssetError::identity_changed);
        if (it->release_attempted) return incomplete(AssetError::release_unverified);
        Identity current;
        if (!read_slot(it->slot, current)) return incomplete(AssetError::unreadable);
        if (!same_resource(it->identity, current) || current.state < 1 || current.state > 2 || current.refs < 0) {
            uncertain_ = true;
            it->identity_lost = true;
            return incomplete(AssetError::identity_changed);
        }
        if (current.refs > 1) return incomplete(AssetError::retained);
    }
    for (auto it = leases_.rbegin(); it != leases_.rend(); ++it) {
        auto& lease = *it;
        if (lease.released) continue;
        // A faulted or unverified release may have already freed other native
        // objects. Never retry that call or infer success from a later observation.
        Identity current;
        if (!read_slot(lease.slot, current)) return incomplete(AssetError::unreadable);
        if (!same_resource(lease.identity, current) || current.state < 1 || current.state > 2 || current.refs < 0) {
            uncertain_ = true;
            lease.identity_lost = true;
            return incomplete(AssetError::identity_changed);
        }
        if (current.refs > 1) return incomplete(AssetError::retained);
        lease.release_attempted = true;
        const bool called = calls_.release(calls_.context, native_.release, lease.slot);
        const bool read = read_slot(lease.slot, current);
        if (!called || !read || !(current == Identity{})) {
            uncertain_ = true;
            return incomplete(!called ? AssetError::native_fault : AssetError::release_unverified);
        }
        lease.released = true;
    }
    if (uncertain_ || owned_count()) {
        return incomplete(AssetError::incomplete);
    }
    state_ = AssetState::rolled_back;
    return {};
}

AssetResult AssetOwner::mark_published() noexcept {
    Operation operation(*this);
    if (operation.error != AssetError::none) return {operation.error};
    if (state_ != AssetState::collecting) return {AssetError::invalid_state};
    if (leases_.size() != families_.size()) return fail(AssetError::incomplete);
    for (auto& lease : leases_) {
        Identity current;
        if (!read_slot(lease.slot, current)) return fail(AssetError::unreadable);
        if (!same_resource(lease.identity, current) || current.state != 2 || current.refs < 1) {
            uncertain_ = true;
            lease.identity_lost = true;
            return fail(AssetError::identity_changed);
        }
    }
    state_ = AssetState::published;
    // Loading and rollback are now closed. Only the resource leases remain
    // necessary; their temporary manager snapshots have no further consumer.
    std::vector<SlotBytes>{}.swap(before_);
    std::vector<SlotBytes>{}.swap(after_);
    return {};
}
} // namespace point_zero::native
