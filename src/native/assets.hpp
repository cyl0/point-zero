#pragma once

#include "memory.hpp"

#include <array>
#include <atomic>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace point_zero::native {
// These addresses and the manager's layout/lifetime must already be admitted by
// the caller (load3519A0, release3507E0). This owner does not admit native code,
// imports, hooks, assets on disk, or the generation's filesystem containment.
struct AssetNative {
    std::uintptr_t manager{}, load{}, release{};
};
struct AssetCalls {
    void* context{};
    // false means the call faulted, not a native null result. Callbacks must not
    // throw; the defaults catch SEH. Native arguments are always load(...,0,0)
    // and release(exact_slot,0), never a group purge.
    bool (*load)(void*, std::uintptr_t entry, std::uintptr_t manager,
                 const char* prefix, const char* name, std::uintptr_t& result) noexcept{};
    bool (*release)(void*, std::uintptr_t entry, std::uintptr_t slot) noexcept{};
    std::uint32_t (*thread_id)(void*) noexcept{};
};
AssetCalls guarded_asset_calls() noexcept;

enum class AssetState { collecting, failed, published, rolled_back, rollback_incomplete };
enum class AssetError {
    none, busy, wrong_thread, invalid_state, unknown_family, duplicate_family,
    unreadable, collision, native_fault, load_failed, ambiguous_allocation,
    identity_changed, retained, release_unverified, incomplete
};
struct AssetResult {
    AssetError error{};
    std::uintptr_t slot{}; // Only a successful load returns a handle.
    explicit operator bool() const noexcept { return error == AssetError::none; }
};

// Initialization-only, single transaction. The caller supplies the finite exact
// family list from its selected feature plan; new families need no magic six-slot
// limit. Names start with '/', prefix ends with '/assets'; source is prefix+name.
// Paths are copied verbatim, never normalized or resolved by doing file I/O.
// Source leaves room for '/afp511' and NUL in AVS's 128-byte mount-path buffer.
//
// The first operation binds the native thread. The caller must serialize native
// manager access and keep all code/storage/context lifetimes alive. Reentrant or
// concurrent mutations are rejected. Inspect getters only between operations.
// Keep this owner and the caller's PZ_Prepared alive through native retirement.
class AssetOwner final {
public:
    static constexpr std::size_t slot_count = 512, slot_stride = 0x518;
    AssetOwner(std::string private_prefix, std::span<const std::string_view> families,
               AssetNative, MemoryView = current_process_memory(),
               AssetCalls = guarded_asset_calls()); // Invalid input/allocation can throw.
    ~AssetOwner() = default; // NEVER calls native code, on any thread.
    AssetOwner(const AssetOwner&) = delete;
    AssetOwner& operator=(const AssetOwner&) = delete;
    AssetOwner(AssetOwner&&) = delete;
    AssetOwner& operator=(AssetOwner&&) = delete;

    AssetResult load(std::string_view family) noexcept;
    AssetResult rollback() noexcept;
    // Commit successfully loaded families before exposing their resources.
    // Published owners retain all leases and cannot load or release them.
    AssetResult mark_published() noexcept;

    AssetState state() const noexcept { return state_; }
    bool uncertain() const noexcept { return uncertain_; }
    // Tracked leases not verified released; not a fresh assertion of native ownership.
    std::size_t owned_count() const noexcept;
    std::uint32_t native_thread() const noexcept { return thread_; }
    std::string_view prefix() const noexcept { return prefix_; }

private:
    using SlotBytes = std::array<std::byte, slot_stride>;
    struct Identity {
        std::array<char, 256> name{}, source{};
        std::int32_t refs{}, state{};
        std::uint32_t id{};
        bool operator==(const Identity&) const = default;
    };
    struct Family { std::string name, source; bool loaded{}; };
    struct Lease {
        std::uintptr_t slot{};
        Identity identity;
        bool release_attempted{}, released{}, identity_lost{};
    };
    class Operation;
    static Identity identity(const SlotBytes&) noexcept;
    static bool vacant(const Identity&) noexcept;
    static bool same_resource(const Identity&, const Identity&) noexcept;
    bool capture(std::vector<SlotBytes>&) const noexcept;
    bool read_slot(std::uintptr_t, Identity&) const noexcept;
    AssetResult fail(AssetError) noexcept;

    const std::string prefix_;
    const AssetNative native_;
    const MemoryView memory_;
    const AssetCalls calls_;
    std::vector<Family> families_;
    std::vector<Lease> leases_;
    std::vector<SlotBytes> before_, after_; // Allocated before any native call.
    std::atomic_flag busy_ = ATOMIC_FLAG_INIT;
    std::uint32_t thread_{};
    AssetState state_ = AssetState::collecting;
    bool uncertain_{}; // Sticky: an unproven side effect is never called clean.
};
} // namespace point_zero::native
