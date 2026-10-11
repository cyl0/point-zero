#pragma once

#include <cstdint>
#include <atomic>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace point_zero::loader {
class FilePin {
public:
    explicit FilePin(const std::filesystem::path&, const std::atomic_bool* cancelled = nullptr,
                     bool allow_removal = false);
    ~FilePin();
    FilePin(FilePin&&) noexcept;
    FilePin& operator=(FilePin&&) noexcept;
    FilePin(const FilePin&) = delete;
    FilePin& operator=(const FilePin&) = delete;
    std::uint64_t size() const;
    std::string read(std::size_t limit) const;
    std::string read_prefix(std::size_t bytes) const;
    void erase(); // Requires a pin opened explicitly for removal.
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// File operations confined to the existing point-zero directory. Callers own
// the update lock. Relative names reject Windows aliases and reparse traversal.
class Files {
public:
    explicit Files(std::filesystem::path root, const std::atomic_bool* cancelled = nullptr);
    ~Files();
    Files(const Files&) = delete;
    Files& operator=(const Files&) = delete;
    const std::filesystem::path& root() const noexcept;
    std::filesystem::path path(std::string_view relative) const;
    std::unique_ptr<FilePin> pin(std::string_view relative) const;
    std::optional<std::string> read(std::string_view relative, std::size_t limit) const;
    void put(std::string_view relative, std::string_view bytes, bool replace = true);
    void erase(std::string_view relative);
    void erase(std::string_view relative, std::string_view expected_sha256);
    bool erase_if_matches(std::string_view relative, std::string_view expected_sha256);
    void rename(std::string_view from, std::string_view to);
    void lock();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace point_zero::loader
