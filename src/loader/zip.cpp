#include "zip.hpp"
#include "release.hpp"

#include <miniz.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

namespace point_zero::loader {
namespace {
constexpr std::size_t entry_limit = 512;
constexpr std::size_t file_limit = static_cast<std::size_t>(release_limits::artifact_bytes);
constexpr std::size_t archive_limit = 512ULL * 1024 * 1024;
constexpr std::size_t path_limit = release_limits::path_bytes;

void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
bool stopped(const std::atomic_bool* cancelled) noexcept {
    return cancelled && cancelled->load(std::memory_order_relaxed);
}
void check_stop(const std::atomic_bool* cancelled) {
    if (stopped(cancelled)) throw std::runtime_error("startup cancelled");
}
// miniz sets m_total_files before allocating the central directory and its
// indices. Bound those allocations there, without interpreting ZIP structures.
bool allocation_allowed(void* opaque, std::size_t count, std::size_t size) noexcept {
    const auto& archive = *static_cast<const mz_zip_archive*>(opaque);
    return archive.m_total_files <= entry_limit && (!size || count <= archive_limit / size);
}
void* allocate(void* opaque, std::size_t count, std::size_t size) noexcept {
    return allocation_allowed(opaque, count, size) ? std::malloc(count * size) : nullptr;
}
void* reallocate(void* opaque, void* memory, std::size_t count, std::size_t size) noexcept {
    return allocation_allowed(opaque, count, size) ? std::realloc(memory, count * size) : nullptr;
}
void release_memory(void*, void* memory) noexcept { std::free(memory); }
struct Reader {
    mz_zip_archive value{};
    Reader() {
        value.m_pAlloc = allocate;
        value.m_pRealloc = reallocate;
        value.m_pFree = release_memory;
        value.m_pAlloc_opaque = &value;
    }
    ~Reader() { if (value.m_pState) (void)mz_zip_reader_end(&value); }
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;
};

struct Output {
    std::string& bytes;
    const std::atomic_bool* cancelled;
    std::size_t written{};
};
std::size_t copy_output(void* opaque, mz_uint64 offset, const void* data, std::size_t size) noexcept {
    auto& output = *static_cast<Output*>(opaque);
    if (stopped(output.cancelled) || offset != output.written ||
        output.written > output.bytes.size() || size > output.bytes.size() - output.written)
        return 0;
    // Deflate already delivers short chunks; chunk stored entries too so a
    // large memcpy does not postpone cancellation until the whole file is copied.
    const auto* input = static_cast<const char*>(data);
    for (std::size_t copied = 0; copied < size;) {
        if (stopped(output.cancelled)) return 0;
        const auto count = std::min<std::size_t>(size - copied, 64 * 1024);
        std::memcpy(output.bytes.data() + output.written, input + copied, count);
        output.written += count;
        copied += count;
    }
    return size;
}
} // namespace

struct Zip::Impl {
    std::string bytes;
    const std::atomic_bool* cancelled;
    Reader reader;
    std::vector<std::string> names;
    std::vector<std::size_t> sizes;

    Impl(std::string input, const std::atomic_bool* stop) : bytes(std::move(input)), cancelled(stop) {
        check_stop(cancelled);
        require(bytes.size() <= archive_limit, "ZIP input size limit exceeded");
        const auto opened = mz_zip_reader_init_mem(&reader.value, bytes.data(), bytes.size(),
                                                   MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY);
        check_stop(cancelled);
        const auto count = reader.value.m_total_files;
        require(count <= entry_limit, "ZIP entry limit exceeded");
        require(opened != MZ_FALSE, "invalid update ZIP");
        names.reserve(count);
        sizes.reserve(count);
        std::size_t total{};
        for (mz_uint index = 0; index < count; ++index) {
            check_stop(cancelled);
            mz_zip_archive_file_stat stat{};
            require(mz_zip_reader_file_stat(&reader.value, index, &stat) != MZ_FALSE,
                    "invalid ZIP entry");
            const auto name_size = mz_zip_reader_get_filename(&reader.value, index, nullptr, 0);
            require(name_size != 0, "invalid ZIP entry name");
            std::string name(name_size, '\0');
            require(mz_zip_reader_get_filename(&reader.value, index, name.data(), name_size) == name_size,
                    "invalid ZIP entry name");
            name.pop_back(); // Keep exact bytes, including any embedded NUL.
            require(std::find(names.begin(), names.end(), name) == names.end(), "duplicate ZIP entry name");
            require(!stat.m_is_encrypted, "encrypted ZIP entry");
            require(stat.m_is_supported && (stat.m_method == 0 || stat.m_method == MZ_DEFLATED),
                    "unsupported ZIP compression method");
            require(stat.m_uncomp_size <= file_limit, "ZIP file size limit exceeded");
            require(stat.m_uncomp_size <= archive_limit - total, "ZIP expanded size limit exceeded");
            check_stop(cancelled);
            const auto size = static_cast<std::size_t>(stat.m_uncomp_size);
            total += size;
            names.push_back(std::move(name));
            sizes.push_back(size);
        }
        check_stop(cancelled);
    }
};

Zip::Zip(std::string bytes, const std::atomic_bool* cancelled)
    : impl_(std::make_unique<Impl>(std::move(bytes), cancelled)) {}
Zip::~Zip() = default;

std::string Zip::read(std::string_view relative_path, std::size_t limit) const {
    check_stop(impl_->cancelled);
    require(relative_path.size() <= path_limit && valid_relative_path(relative_path), "invalid ZIP relative path");
    const auto it = std::find(impl_->names.begin(), impl_->names.end(), relative_path);
    require(it != impl_->names.end(), "ZIP file not found");
    const auto index = static_cast<std::size_t>(it - impl_->names.begin());
    // miniz returns success without reading directory contents. Reject that
    // shortcut for a requested member while ignoring unused entry attributes.
    require(!mz_zip_reader_is_file_a_directory(&impl_->reader.value, static_cast<mz_uint>(index)),
            "ZIP requested member is a directory");
    const auto size = impl_->sizes[index];
    require(size <= limit, "ZIP read size limit exceeded");
    check_stop(impl_->cancelled);
    std::string result(size, '\0');
    Output output{result, impl_->cancelled};
    check_stop(impl_->cancelled);
    const auto extracted = mz_zip_reader_extract_to_callback(&impl_->reader.value,
        static_cast<mz_uint>(index), copy_output, &output, 0);
    check_stop(impl_->cancelled); // preserve cancellation instead of a generic callback error.
    require(extracted != MZ_FALSE && output.written == size, "ZIP extraction or CRC check failed");
    return result;
}

} // namespace point_zero::loader
