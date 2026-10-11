#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace point_zero::loader {
// Owns the archive bytes and reads exact, unique member names into memory.
// Requested names must be plain relative paths; unused entry names and file
// attributes do not define any filesystem policy. Nothing is written to disk.
// The optional cancellation flag must outlive this object.
// Reads belong to the serialized startup owner; only cancellation is shared.
class Zip {
public:
    explicit Zip(std::string bytes, const std::atomic_bool* cancelled = nullptr);
    ~Zip();
    Zip(const Zip&) = delete;
    Zip& operator=(const Zip&) = delete;
    std::string read(std::string_view relative_path, std::size_t limit) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace point_zero::loader
