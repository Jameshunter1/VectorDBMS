#include <core_engine/common/logger.hpp>
#include <core_engine/storage/disk_manager.hpp>
#include <core_engine/storage/page.hpp> // Ensure Page and PageId are defined
#define CORE_ENGINE_DISK_MANAGER_HPP
#include <array>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <future>

#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <algorithm>
#include <cstdio>
#include <limits>

// For Windows-specific file operations
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <io.h> // For _lseeki64, _open, _setmode etc.
#include <windows.h>
#undef min
#undef max

#define OFFSET_TYPE __int64 // Use 64-bit offset type on Windows
#else
#include <fcntl.h>
#include <unistd.h>
#define FSEEK fseeko // Use 64-bit fseek on Unix-like systems
#define OFFSET_TYPE off_t
#endif

namespace core_engine {

namespace fs = std::filesystem;

DiskManager::DiskManager(std::filesystem::path db_file, Options options)
    : db_file_(std::move(db_file)), options_(options), stats_{} {
}

DiskManager::~DiskManager() {
  Close();
}
Status DiskManager::Open() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (is_open_) {
    return Status::AlreadyExists("DiskManager already open");
  }

  // Create parent directory if it doesn't exist
  fs::path parent = db_file_.parent_path();
  if (!parent.empty() && !fs::exists(parent)) {
    std::error_code ec;
    fs::create_directories(parent, ec);
    if (ec) {
      return Status::IoError("Failed to create directory: " + ec.message());
    }
  }

  auto direct_status = OpenDirectLocked();
  if (direct_status.ok()) {
    use_direct_io_ = true;
  } else if (direct_status.code() != StatusCode::kUnimplemented) {
    return direct_status;
  } else {
    auto buffered_status = OpenBufferedLocked();
    if (!buffered_status.ok()) {
      return buffered_status;
    }
    use_direct_io_ = false;
  }

#if defined(CORE_ENGINE_HAS_IO_URING)
  if (options_.enable_io_uring) {
    const Status io_status = InitializeIoUringLocked();
    if (!io_status.ok()) {
      Log(LogLevel::kWarn,
          "io_uring disabled for " + db_file_.string() + ": " + std::string(io_status.message()));
      options_.enable_io_uring = false;
    }
  }
#endif

  if (num_pages_ == 0) {
    // Initialize reserved Page 0 (Header/Superblock)
    // Use Page object to ensure 4KB alignment required for Direct I/O
    Page page;
    page.Reset(kInvalidPageId); // Zero out data, set ID to 0

    auto status = WriteRawLocked(0, page.GetRawPage(), kPageSize);
    if (!status.ok()) {
      // Close handles on failure
      if (use_direct_io_) {
#ifdef _WIN32
        if (direct_file_handle_ != INVALID_HANDLE_VALUE) {
          CloseHandle(static_cast<HANDLE>(direct_file_handle_));
          direct_file_handle_ = INVALID_HANDLE_VALUE;
        }
#else
        if (file_descriptor_ >= 0) {
          ::close(file_descriptor_);
          file_descriptor_ = -1;
        }
#endif
      } else if (file_handle_) {
        std::fclose(file_handle_);
        file_handle_ = nullptr;
      }
      return status;
    }
    num_pages_ = 1;
  }

  is_open_ = true;

  Log(LogLevel::kDebug,
      "DiskManager opened: " + db_file_.string() + " (" + std::to_string(num_pages_) +
          " pages, direct_io=" + std::string(use_direct_io_ ? "true" : "false") + ")");

  return Status::Ok();
}
void DiskManager::Close() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!is_open_) {
    return;
  }

#if defined(CORE_ENGINE_HAS_IO_URING)
  ShutdownIoUringLocked();
#endif

  if (use_direct_io_) {
#ifdef _WIN32
    if (direct_file_handle_ != INVALID_HANDLE_VALUE) {
      FlushFileBuffers(static_cast<HANDLE>(direct_file_handle_));
      CloseHandle(static_cast<HANDLE>(direct_file_handle_));
      direct_file_handle_ = INVALID_HANDLE_VALUE;
    }
#else
    if (file_descriptor_ >= 0) {
      ::close(file_descriptor_);
      file_descriptor_ = -1;
    }
#endif
  } else if (file_handle_) {
    std::fflush(file_handle_);
    std::fclose(file_handle_);
    file_handle_ = nullptr;
#if !defined(_WIN32)
    file_descriptor_ = -1;
#endif
  }

  is_open_ = false;
  use_direct_io_ = false;

  Log(LogLevel::kDebug, "DiskManager closed: " + db_file_.string());
}
Status DiskManager::OpenBufferedLocked() {
  const bool file_exists = fs::exists(db_file_);

#ifdef _WIN32
  errno_t err = _wfopen_s(&file_handle_, db_file_.wstring().c_str(), L"rb+");
  if (err != 0 && !file_exists) {
    err = _wfopen_s(&file_handle_, db_file_.wstring().c_str(), L"wb+");
  }
#else
  file_handle_ = std::fopen(db_file_.c_str(), "rb+");
  if (!file_handle_ && !file_exists) {
    file_handle_ = std::fopen(db_file_.c_str(), "wb+");
  }
#endif

  if (!file_handle_) {
    return Status::IoError("Failed to open file: " + db_file_.string());
  }

#ifdef _WIN32
  if (_fseeki64(file_handle_, 0, SEEK_END) != 0) {
    std::fclose(file_handle_);
    file_handle_ = nullptr;
    return Status::IoError("Failed to seek file end");
  }

  const __int64 file_size = _ftelli64(file_handle_);
  if (file_size < 0) {
    std::fclose(file_handle_);
    file_handle_ = nullptr;
    return Status::IoError("Failed to determine file size");
  }

  _fseeki64(file_handle_, 0, SEEK_SET);
#else
  if (fseeko(file_handle_, 0, SEEK_END) != 0) {
    std::fclose(file_handle_);
    file_handle_ = nullptr;
    return Status::IoError("Failed to seek file end");
  }

  const off_t file_size = ftello(file_handle_);
  if (file_size < 0) {
    std::fclose(file_handle_);
    file_handle_ = nullptr;
    return Status::IoError("Failed to determine file size");
  }

  fseeko(file_handle_, 0, SEEK_SET);
#endif

  if (file_size % kPageSize != 0) {
    std::fclose(file_handle_);
    file_handle_ = nullptr;
    return Status::Corruption("File size is not a multiple of page size");
  }

  num_pages_ = static_cast<PageId>(file_size / kPageSize);

#if defined(CORE_ENGINE_HAS_IO_URING)
  file_descriptor_ = fileno(file_handle_);
  if (file_descriptor_ < 0) {
    std::fclose(file_handle_);
    file_handle_ = nullptr;
    return Status::IoError("Failed to retrieve file descriptor for io_uring");
  }
#else
  file_descriptor_ = -1;
#endif

#ifdef _WIN32
  direct_file_handle_ = INVALID_HANDLE_VALUE;
#endif

  return Status::Ok();
}
Status DiskManager::OpenDirectLocked() {
#ifdef _WIN32
  const DWORD access = GENERIC_READ | GENERIC_WRITE;
  const DWORD share = FILE_SHARE_READ;
  const DWORD flags = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH;
  HANDLE handle =
      CreateFileW(db_file_.wstring().c_str(), access, share, nullptr, OPEN_ALWAYS, flags, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    const DWORD err = GetLastError();
    if (err == ERROR_INVALID_PARAMETER || err == ERROR_NOT_SUPPORTED) {
      return Status::Unimplemented("Unbuffered I/O not supported on this volume");
    }
    return Status::IoError("CreateFileW failed: error=" + std::to_string(err));
  }

  LARGE_INTEGER size{};
  if (!GetFileSizeEx(handle, &size)) {
    CloseHandle(handle);
    return Status::IoError("Failed to determine file size");
  }
  if (size.QuadPart % kPageSize != 0) {
    CloseHandle(handle);
    return Status::Corruption("File size is not a multiple of page size");
  }

  direct_file_handle_ = handle;
  file_handle_ = nullptr;
  file_descriptor_ = -1;
  num_pages_ = static_cast<PageId>(size.QuadPart / kPageSize);
  return Status::Ok();
#else
#if defined(O_DIRECT)
  int flags = O_RDWR | O_CREAT | O_DIRECT;
#else
  return Status::Unimplemented("O_DIRECT not available on this platform");
#endif
#if defined(O_SYNC)
  flags |= O_SYNC;
#endif

  int fd = ::open(db_file_.c_str(), flags, 0644);
  if (fd < 0) {
    if (errno == EINVAL || errno == ENOTSUP) {
      return Status::Unimplemented("Direct I/O not supported for this filesystem");
    }
    return Status::IoError("Failed to open file with direct I/O: " +
                           std::string(std::strerror(errno)));
  }

  off_t size = ::lseek(fd, 0, SEEK_END);
  if (size < 0) {
    ::close(fd);
    return Status::IoError("Failed to determine file size");
  }
  if (size % kPageSize != 0) {
    ::close(fd);
    return Status::Corruption("File size is not a multiple of page size");
  }
  ::lseek(fd, 0, SEEK_SET);

  file_descriptor_ = fd;
  file_handle_ = nullptr;
  num_pages_ = static_cast<PageId>(size / kPageSize);
  return Status::Ok();
#endif
}
Status DiskManager::ReadPage(PageId page_id, Page* page) {
  if (!is_open_) {
    return Status::IoError("DiskManager not open");
  }

  // Use the helper to validate page_id range
  if (!IsValidPageId(page_id)) {
    return Status::NotFound("Invalid page ID: " + std::to_string(page_id));
  }

  const std::int64_t offset = PageIdToOffset(page_id);
  std::lock_guard<std::mutex> lock(mutex_);

  // FIX: Delegate to ReadRawLocked to handle 64-bit offsets and Direct I/O paths
  Status status = ReadRawLocked(offset, page->GetRawPage(), kPageSize);
  if (!status.ok()) {
    return status;
  }

  // Always validate the data read from disk
  status = ValidateReadResult(page_id, page);
  if (!status.ok()) {
    return status;
  }

  stats_.total_reads++;
  return Status::Ok();
}
Status DiskManager::ReadPagesBatch(std::span<PageReadRequest> requests) {
  if (requests.empty()) {
    return Status::Ok();
  }

  for (const auto& request : requests) {
    if (!request.page) {
      return Status::InvalidArgument("Page pointer is null");
    }
  }

  std::lock_guard<std::mutex> lock(mutex_);

  if (!is_open_) {
    return Status::Internal("DiskManager not open");
  }

  return ProcessReadRequestsLocked(requests);
}
Status DiskManager::WritePage(PageId page_id, const Page* page) {
  if (!is_open_) {
    return Status::IoError("DiskManager not open");
  }

  // Use IsValidPageId for consistency
  if (!IsValidPageId(page_id)) {
    return Status::NotFound("Invalid page ID: " + std::to_string(page_id));
  }

  // Calculate the offset for the page in the file
  const std::int64_t offset = PageIdToOffset(page_id);

  // Update the checksum before writing
  // Note: const_cast is necessary because we're updating metadata even on a "const" page write
  const_cast<Page*>(page)->UpdateChecksum();

  std::lock_guard<std::mutex> lock(mutex_);

  // FIX: Use WriteRawLocked to handle both buffered and Direct I/O correctly
  Status status = WriteRawLocked(offset, page->GetRawPage(), kPageSize);
  if (!status.ok()) {
    return status;
  }

  stats_.total_writes++;
  return Status::Ok();
}
Status DiskManager::WritePagesBatch(std::span<PageWriteRequest> requests) {
  if (requests.empty()) {
    return Status::Ok();
  }

  for (const auto& request : requests) {
    if (!request.page) {
      return Status::InvalidArgument("Page pointer is null");
    }
    if (!request.page->VerifyChecksum()) {
      return Status::InvalidArgument(
          "Page checksum not valid - call UpdateChecksum() before WritePage");
    }
  }

  std::lock_guard<std::mutex> lock(mutex_);

  if (!is_open_) {
    return Status::Internal("DiskManager not open");
  }

  return ProcessWriteRequestsLocked(requests);
}
Status DiskManager::ProcessReadRequestsLocked(std::span<PageReadRequest> requests) {
  for (const auto& request : requests) {
    if (!IsValidPageId(request.page_id)) {
      return Status::InvalidArgument("Invalid page_id: " + std::to_string(request.page_id));
    }
  }

#if defined(CORE_ENGINE_HAS_IO_URING)
  if (options_.enable_io_uring && io_uring_initialized_) {
    std::vector<IoTask> tasks;
    tasks.reserve(requests.size());
    for (auto& request : requests) {
      const int buffer_index = GetBufferIndex(request.page);
      tasks.push_back({IoTask::Type::kRead, request.page_id, request.page, nullptr, buffer_index});
    }
    return SubmitIoTasksLocked(tasks);
  }
#endif

  for (auto& request : requests) {
    auto status = PerformSingleReadLocked(request.page_id, request.page);
    if (!status.ok()) {
      return status;
    }
  }
  return Status::Ok();
}
Status DiskManager::ReadContiguous(PageId first_page_id, void* buffer, std::size_t page_count) {
  if (page_count == 0) {
    return Status::Ok();
  }
  if (!buffer) {
    return Status::InvalidArgument("Buffer pointer is null");
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (!is_open_) {
    return Status::Internal("DiskManager not open");
  }

  if (first_page_id == kInvalidPageId) {
    return Status::InvalidArgument("Invalid first_page_id");
  }

  const std::uint64_t last_page = static_cast<std::uint64_t>(first_page_id) + page_count - 1;
  if (last_page > static_cast<std::uint64_t>(num_pages_)) {
    return Status::InvalidArgument("Requested range exceeds database size");
  }

  if (page_count > (std::numeric_limits<std::size_t>::max)() / kPageSize) {
    if (last_page >= static_cast<std::uint64_t>(num_pages_)) {
    }

    const std::int64_t offset = PageIdToOffset(first_page_id);
    const std::size_t total_bytes = page_count * kPageSize;
    auto status = ReadRawLocked(offset, buffer, total_bytes);
    if (status.ok()) {
      stats_.total_reads += page_count;
    }
    return status;
  }
}
Status DiskManager::WriteContiguous(PageId first_page_id, const void* buffer,
                                    std::size_t page_count) {
  if (page_count == 0) {
    return Status::Ok();
  }
  if (!buffer) {
    return Status::InvalidArgument("Buffer pointer is null");
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (!is_open_) {
    return Status::Internal("DiskManager not open");
  }

  if (first_page_id == kInvalidPageId) {
    return Status::InvalidArgument("Invalid first_page_id");
  }

  const std::uint64_t last_page = static_cast<std::uint64_t>(first_page_id) + page_count - 1;
  const std::uint64_t first_page = static_cast<std::uint64_t>(first_page_id);
  const std::uint64_t max_allowed = static_cast<std::uint64_t>(num_pages_) + 1;
  if (first_page > max_allowed) {
    return Status::InvalidArgument("Cannot create gaps in page file");
  }

  if (page_count > (std::numeric_limits<std::size_t>::max)() / kPageSize) {
    return Status::InvalidArgument("Requested range is too large");
  }

  const std::int64_t offset = PageIdToOffset(first_page_id);
  const std::size_t total_bytes = page_count * kPageSize;
  auto status = WriteRawLocked(offset, buffer, total_bytes);
  if (status.ok()) {
    if (last_page > static_cast<std::uint64_t>(num_pages_)) {
      num_pages_ = static_cast<PageId>(last_page);
    }
    stats_.total_writes += page_count;
  }
  return status;
}
Status DiskManager::ProcessWriteRequestsLocked(std::span<PageWriteRequest> requests) {
  for (const auto& request : requests) {
    if (!IsValidPageId(request.page_id)) {
      return Status::InvalidArgument("Invalid page_id: " + std::to_string(request.page_id));
    }
  }

#if defined(CORE_ENGINE_HAS_IO_URING)
  if (options_.enable_io_uring && io_uring_initialized_) {
    std::vector<IoTask> tasks;
    tasks.reserve(requests.size());
    for (auto& request : requests) {
      const int buffer_index = GetBufferIndex(request.page);
      tasks.push_back({IoTask::Type::kWrite, request.page_id, nullptr, request.page, buffer_index});
    }
    auto status = SubmitIoTasksLocked(tasks);
    if (!status.ok()) {
      return status;
    }
    auto flush_status = FlushFileLocked();
    if (!flush_status.ok()) {
      return flush_status;
    }
    stats_.total_writes += requests.size();
    return Status::Ok();
  }
#endif

  for (auto& request : requests) {
    auto status = PerformSingleWriteLocked(request.page_id, request.page);
    if (!status.ok()) {
      return status;
    }
  }

  return Status::Ok();
}
Status DiskManager::PerformSingleReadLocked(PageId page_id, Page* page) {
  const std::int64_t offset = PageIdToOffset(page_id);
  auto status = ReadRawLocked(offset, page->GetRawPage(), kPageSize);
  if (!status.ok()) {
    return status;
  }

  status = ValidateReadResult(page_id, page);
  if (!status.ok()) {
    return status;
  }

  ++stats_.total_reads;
  return Status::Ok();
}
Status DiskManager::PerformSingleWriteLocked(PageId page_id, const Page* page) {
  const std::int64_t offset = PageIdToOffset(page_id);
  auto status = WriteRawLocked(offset, page->GetRawPage(), kPageSize);
  if (!status.ok()) {
    return status;
  }

  ++stats_.total_writes;
  return Status::Ok();
}
Status DiskManager::ValidateReadResult(PageId page_id, Page* page) {
  if (!page->VerifyChecksum()) {
    ++stats_.checksum_failures;
    return Status::Corruption("Checksum mismatch for page " + std::to_string(page_id));
  }

  if (page->GetPageId() != page_id && page->GetPageId() != kInvalidPageId) {
    return Status::Corruption("Page ID mismatch: expected " + std::to_string(page_id) + ", got " +
                              std::to_string(page->GetPageId()));
  }

  return Status::Ok();
}

// Page 0 is reserved, so first valid page is 1
// Page 0 exists and is the superblock
PageId DiskManager::AllocatePage() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!is_open_) {
    return kInvalidPageId;
  }

  const PageId new_page_id = num_pages_;
  ++num_pages_;
  ++stats_.total_allocations;

  return new_page_id;
}

Status DiskManager::Sync() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!is_open_) {
    return Status::Internal("DiskManager not open");
  }

  return FlushFileLocked();
}

DiskManager::Stats DiskManager::GetStats() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return stats_;
}

bool DiskManager::IsValidPageId(PageId page_id) const {
  // Page 0 is reserved
  if (page_id == kInvalidPageId) {
    return false;
  }

  // Page must be within file bounds (page_id 1 to num_pages_ inclusive)
  if (page_id > num_pages_) {
    return false;
  }

  return true;
}

std::int64_t DiskManager::PageIdToOffset(PageId page_id) const {
  return static_cast<std::int64_t>(page_id) * kPageSize;
}

Status DiskManager::ReadRawLocked(std::int64_t offset, void* buffer, std::size_t size) {
  if (size == 0) {
    return Status::Ok();
  }
  if (!buffer) {
    return Status::InvalidArgument("Buffer pointer is null");
  }

  if (use_direct_io_) {
    if (offset % kPageSize != 0 || size % kPageSize != 0) {
      return Status::InvalidArgument("Direct I/O requires 4 KB aligned offset/size");
    }
    const auto address = reinterpret_cast<std::uintptr_t>(buffer);
    if (address % kPageSize != 0) {
      return Status::InvalidArgument("Direct I/O requires 4 KB aligned buffers");
    }

#ifdef _WIN32
    if (direct_file_handle_ == INVALID_HANDLE_VALUE) {
      return Status::Internal("Invalid direct I/O handle");
    }

    HANDLE handle = static_cast<HANDLE>(direct_file_handle_);
    std::uint8_t* cursor = static_cast<std::uint8_t*>(buffer);
    std::size_t remaining = size;
    std::int64_t current_offset = offset;
    while (remaining > 0) {
      const DWORD chunk = static_cast<DWORD>(
          std::min(remaining, static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
      OVERLAPPED overlapped{};
      overlapped.Offset = static_cast<DWORD>(current_offset & 0xffffffffu);
      overlapped.OffsetHigh = static_cast<DWORD>((current_offset >> 32) & 0xffffffffu);

      DWORD bytes_read = 0;
      if (!ReadFile(handle, cursor, chunk, &bytes_read, &overlapped)) {
        return Status::IoError("ReadFile failed: error=" + std::to_string(GetLastError()));
      }
      if (bytes_read != chunk) {
        return Status::IoError("Short read encountered during direct I/O");
      }

      remaining -= bytes_read;
      cursor += bytes_read;
      current_offset += bytes_read;
    }
#else
    if (file_descriptor_ < 0) {
      return Status::Internal("Invalid direct I/O file descriptor");
    }

    std::uint8_t* cursor = static_cast<std::uint8_t*>(buffer);
    std::size_t remaining = size;
    std::int64_t current_offset = offset;
    while (remaining > 0) {
      const ssize_t bytes_read = ::pread(file_descriptor_, cursor, remaining, current_offset);
      if (bytes_read < 0) {
        return Status::IoError("pread failed: " + std::string(std::strerror(errno)));
      }
      if (bytes_read == 0) {
        return Status::IoError("Unexpected EOF during direct read");
      }
      remaining -= static_cast<std::size_t>(bytes_read);
      cursor += bytes_read;
      current_offset += bytes_read;
    }
#endif
    return Status::Ok();
  }

  if (!file_handle_) {
    return Status::Internal("Buffered file handle is null");
  }

#ifdef _WIN32
  if (_fseeki64(file_handle_, offset, SEEK_SET) != 0) {
    return Status::IoError("Failed to seek for buffered read");
  }
#else
  if (fseeko(file_handle_, static_cast<off_t>(offset), SEEK_SET) != 0) {
    return Status::IoError("Failed to seek for buffered read");
  }
#endif

  const std::size_t bytes_read = std::fread(buffer, 1, size, file_handle_);
  if (bytes_read != size) {
    return Status::IoError("Buffered read returned insufficient bytes");
  }
  return Status::Ok();
}

Status DiskManager::WriteRawLocked(std::int64_t offset, const void* buffer, std::size_t size) {
  if (size == 0) {
    return Status::Ok();
  }
  if (!buffer) {
    return Status::InvalidArgument("Buffer pointer is null");
  }

  if (use_direct_io_) {
    if (offset % kPageSize != 0 || size % kPageSize != 0) {
      return Status::InvalidArgument("Direct I/O requires 4 KB aligned offset/size");
    }
    const auto address = reinterpret_cast<std::uintptr_t>(buffer);
    if (address % kPageSize != 0) {
      return Status::InvalidArgument("Direct I/O requires 4 KB aligned buffers");
    }

#ifdef _WIN32
    if (direct_file_handle_ == INVALID_HANDLE_VALUE) {
      return Status::Internal("Invalid direct I/O handle");
    }

    HANDLE handle = static_cast<HANDLE>(direct_file_handle_);
    const std::uint8_t* cursor = static_cast<const std::uint8_t*>(buffer);
    std::size_t remaining = size;
    std::int64_t current_offset = offset;
    while (remaining > 0) {
      const DWORD chunk = static_cast<DWORD>(
          std::min(remaining, static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
      OVERLAPPED overlapped{};
      overlapped.Offset = static_cast<DWORD>(current_offset & 0xffffffffu);
      overlapped.OffsetHigh = static_cast<DWORD>((current_offset >> 32) & 0xffffffffu);

      DWORD bytes_written = 0;
      if (!WriteFile(handle, cursor, chunk, &bytes_written, &overlapped)) {
        return Status::IoError("WriteFile failed: error=" + std::to_string(GetLastError()));
      }
      if (bytes_written != chunk) {
        return Status::IoError("Short write encountered during direct I/O");
      }

      remaining -= bytes_written;
      cursor += bytes_written;
      current_offset += bytes_written;
    }
#else
    if (file_descriptor_ < 0) {
      return Status::Internal("Invalid direct I/O file descriptor");
    }

    const std::uint8_t* cursor = static_cast<const std::uint8_t*>(buffer);
    std::size_t remaining = size;
    std::int64_t current_offset = offset;
    while (remaining > 0) {
      const ssize_t bytes_written = ::pwrite(file_descriptor_, cursor, remaining, current_offset);
      if (bytes_written < 0) {
        return Status::IoError("pwrite failed: " + std::string(std::strerror(errno)));
      }
      if (bytes_written == 0) {
        return Status::IoError("Unexpected zero-byte write during direct I/O");
      }
      remaining -= static_cast<std::size_t>(bytes_written);
      cursor += bytes_written;
      current_offset += bytes_written;
    }
#endif
  } else {
    if (!file_handle_) {
      return Status::Internal("Buffered file handle is null");
    }

#ifdef _WIN32
    if (_fseeki64(file_handle_, offset, SEEK_SET) != 0) {
      return Status::IoError("Failed to seek for buffered write");
    }
#else
    if (fseeko(file_handle_, static_cast<off_t>(offset), SEEK_SET) != 0) {
      return Status::IoError("Failed to seek for buffered write");
    }
#endif

    const std::size_t bytes_written = std::fwrite(buffer, 1, size, file_handle_);
    if (bytes_written != size) {
      return Status::IoError("Buffered write returned insufficient bytes");
    }
  }

  return FlushFileLocked();
}

Status DiskManager::FlushFileLocked() {
  if (use_direct_io_) {
#ifdef _WIN32
    if (direct_file_handle_ == INVALID_HANDLE_VALUE) {
      return Status::Internal("Invalid direct I/O handle");
    }
    HANDLE handle = static_cast<HANDLE>(direct_file_handle_);
    if (!FlushFileBuffers(handle)) {
      return Status::IoError("FlushFileBuffers failed: error=" + std::to_string(GetLastError()));
    }
#else
    if (file_descriptor_ < 0) {
      return Status::Internal("Invalid direct I/O file descriptor");
    }
    if (fsync(file_descriptor_) != 0) {
      return Status::IoError("fsync failed: " + std::string(std::strerror(errno)));
    }
#endif
    return Status::Ok();
  }

  if (!file_handle_) {
    return Status::Internal("Buffered file handle is null");
  }

  if (std::fflush(file_handle_) != 0) {
    return Status::IoError("fflush failed");
  }

#ifdef _WIN32
  const int fd = _fileno(file_handle_);
  if (fd < 0) {
    return Status::IoError("Invalid file descriptor for FlushFileBuffers");
  }
  HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
  if (handle == INVALID_HANDLE_VALUE) {
    return Status::IoError("Failed to translate FILE* to HANDLE");
  }
  if (!FlushFileBuffers(handle)) {
    return Status::IoError("FlushFileBuffers failed: error=" + std::to_string(GetLastError()));
  }
#else
  const int fd = fileno(file_handle_);
  if (fd < 0) {
    return Status::IoError("Invalid file descriptor for fsync");
  }
  if (fsync(fd) != 0) {
    return Status::IoError("fsync failed: " + std::string(std::strerror(errno)));
  }
#endif

  return Status::Ok();
}

#if defined(CORE_ENGINE_HAS_IO_URING)
Status DiskManager::InitializeIoUringLocked() {
  if (io_uring_initialized_ || !options_.enable_io_uring) {
    return Status::Ok();
  }

  if (file_descriptor_ < 0) {
    return Status::Internal("Invalid file descriptor for io_uring initialization");
  }

  const unsigned depth = std::max(32u, options_.io_uring_queue_depth);
  const int ret = io_uring_queue_init(depth, &io_ring_, 0);
  if (ret < 0) {
    return Status::IoError(std::string("io_uring_queue_init failed: ") + std::strerror(-ret));
  }

  io_uring_queue_depth_ = depth;
  io_uring_initialized_ = true;
  return Status::Ok();
}

void DiskManager::ShutdownIoUringLocked() {
  if (io_uring_initialized_) {
    // Unregister fixed buffers before shutting down io_uring
    UnregisterFixedBuffersLocked();
    io_uring_queue_exit(&io_ring_);
    io_uring_initialized_ = false;
  }
}

Status DiskManager::SubmitIoTasksLocked(std::span<IoTask> tasks) {
  if (tasks.empty()) {
    return Status::Ok();
  }

  if (!io_uring_initialized_ || !options_.enable_io_uring) {
    return Status::Internal("io_uring not initialized");
  }

  if (file_descriptor_ < 0) {
    return Status::Internal("Invalid file descriptor for io_uring");
  }

  auto process_completion = [&](io_uring_cqe* cqe) -> Status {
    auto* completed = static_cast<IoTask*>(io_uring_cqe_get_data(cqe));
    if (!completed) {
      return Status::Internal("Missing io_uring completion context");
    }

    if (cqe->res < 0) {
      return Status::IoError(std::string("io_uring operation failed: ") + std::strerror(-cqe->res));
    }

    if (static_cast<std::size_t>(cqe->res) != kPageSize) {
      return Status::IoError("Partial I/O detected for page " + std::to_string(completed->page_id));
    }

    if (completed->type == IoTask::Type::kRead) {
      auto status = ValidateReadResult(completed->page_id, completed->read_page);
      if (!status.ok()) {
        return status;
      }
      ++stats_.total_reads;
    } else {
      ++stats_.total_writes;
    }

    return Status::Ok();
  };

  std::size_t next_request = 0;
  unsigned pending = 0;

  while (next_request < tasks.size() || pending > 0) {
    while (next_request < tasks.size() && pending < io_uring_queue_depth_) {
      io_uring_sqe* sqe = io_uring_get_sqe(&io_ring_);
      if (!sqe) {
        break;
      }

      IoTask* task = &tasks[next_request];
      const std::int64_t offset = PageIdToOffset(task->page_id);

      if (task->type == IoTask::Type::kRead) {
        if (task->buffer_index >= 0 && fixed_buffers_registered_) {
          // Use fixed-buffer read for zero-copy DMA
          io_uring_prep_read_fixed(sqe, file_descriptor_, task->read_page->GetRawPage(), kPageSize,
                                   offset, task->buffer_index);
        } else {
          // Fallback to dynamic buffer
          io_uring_prep_read(sqe, file_descriptor_, task->read_page->GetRawPage(), kPageSize,
                             offset);
        }
      } else {
        if (task->buffer_index >= 0 && fixed_buffers_registered_) {
          // Use fixed-buffer write for zero-copy DMA
          io_uring_prep_write_fixed(sqe, file_descriptor_, task->write_page->GetRawPage(),
                                    kPageSize, offset, task->buffer_index);
        } else {
          // Fallback to dynamic buffer
          io_uring_prep_write(sqe, file_descriptor_, task->write_page->GetRawPage(), kPageSize,
                              offset);
        }
      }

      io_uring_sqe_set_data(sqe, task);
      ++pending;
      ++next_request;
    }

    if (pending == 0) {
      continue;
    }

    const int submit_result = io_uring_submit(&io_ring_);
    if (submit_result < 0) {
      return Status::IoError(std::string("io_uring_submit failed: ") +
                             std::strerror(-submit_result));
    }

    io_uring_cqe* cqe = nullptr;
    const int wait_result = io_uring_wait_cqe(&io_ring_, &cqe);
    if (wait_result < 0) {
      return Status::IoError(std::string("io_uring_wait_cqe failed: ") +
                             std::strerror(-wait_result));
    }

    Status completion_status = process_completion(cqe);
    io_uring_cqe_seen(&io_ring_, cqe);
    --pending;
    if (!completion_status.ok()) {
      return completion_status;
    }

    while (pending > 0 && io_uring_peek_cqe(&io_ring_, &cqe) == 0) {
      completion_status = process_completion(cqe);
      io_uring_cqe_seen(&io_ring_, cqe);
      --pending;
      if (!completion_status.ok()) {
        return completion_status;
      }
    }
  }

  return Status::Ok();
}

Status DiskManager::RegisterFixedBuffersLocked(std::span<Page> buffers) {
  if (buffers.empty()) {
    return Status::InvalidArgument("Cannot register empty buffer span");
  }

  if (fixed_buffers_registered_) {
    return Status::AlreadyExists("Fixed buffers already registered");
  }

  if (!io_uring_initialized_ || !options_.enable_io_uring) {
    return Status::Internal("io_uring not initialized");
  }

  // Validate 4KB page alignment requirement for io_uring fixed buffers
  if (reinterpret_cast<std::uintptr_t>(buffers.data()) % 4096 != 0) {
    return Status::InvalidArgument("Buffers must be 4KB-aligned for io_uring fixed buffers");
  }

  // Create iovec array for registration
  std::vector<iovec> iovecs;
  iovecs.reserve(buffers.size());

  for (const auto& page : buffers) {
    iovec iov;
    iov.iov_base = const_cast<char*>(page.GetRawPage());
    iov.iov_len = kPageSize;
    iovecs.push_back(iov);
  }

  // Register buffers with io_uring (atomic operation - all-or-nothing)
  const int ret = io_uring_register_buffers(&io_ring_, iovecs.data(), iovecs.size());
  if (ret < 0) {
    // io_uring_register_buffers is atomic; no partial registration occurs
    return Status::IoError(std::string("io_uring_register_buffers failed: ") + std::strerror(-ret));
  }

  // Store registration metadata
  fixed_buffers_registered_ = true;
  fixed_buffer_base_ = buffers.data();
  fixed_buffer_count_ = buffers.size();

  Log(LogLevel::kInfo,
      "Registered " + std::to_string(buffers.size()) + " fixed buffers for zero-copy I/O");

  return Status::Ok();
}

void DiskManager::UnregisterFixedBuffersLocked() {
  if (!fixed_buffers_registered_) {
    return;
  }

  if (io_uring_initialized_) {
    io_uring_unregister_buffers(&io_ring_);
  }

  fixed_buffers_registered_ = false;
  fixed_buffer_base_ = nullptr;
  fixed_buffer_count_ = 0;

  Log(LogLevel::kDebug, "Unregistered fixed buffers");
}

int DiskManager::GetBufferIndex(const Page* page) const {
  if (!fixed_buffers_registered_ || !page || !fixed_buffer_base_) {
    return -1; // Use dynamic buffer
  }

  // Calculate buffer index from pointer arithmetic
  const std::ptrdiff_t diff = page - fixed_buffer_base_;

  // Validate the page is within the registered buffer range
  if (diff < 0 || static_cast<std::size_t>(diff) >= fixed_buffer_count_) {
    return -1; // Page not in registered buffer range, use dynamic buffer
  }

  return static_cast<int>(diff);
}
#endif

Status DiskManager::RegisterFixedBuffers(std::span<Page> buffers) {
#if defined(CORE_ENGINE_HAS_IO_URING)
  std::lock_guard<std::mutex> lock(mutex_);
  return RegisterFixedBuffersLocked(buffers);
#else
  (void)buffers; // Suppress unused parameter warning
  return Status::Unimplemented("io_uring not available on this platform");
#endif
}

void DiskManager::UnregisterFixedBuffers() {
#if defined(CORE_ENGINE_HAS_IO_URING)
  std::lock_guard<std::mutex> lock(mutex_);
  UnregisterFixedBuffersLocked();
#endif
}

} // namespace core_engine
