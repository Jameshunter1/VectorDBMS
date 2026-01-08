#include <core_engine/common/logger.hpp>
#include <core_engine/storage/disk_manager.hpp>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace core_engine {

namespace fs = std::filesystem;

DiskManager::DiskManager(std::filesystem::path db_file, Options options)
    : db_file_(std::move(db_file)), file_handle_(nullptr), is_open_(false), num_pages_(0),
      options_(std::move(options)), file_descriptor_(-1), use_direct_io_(false)
#if defined(CORE_ENGINE_HAS_IO_URING)
      ,
      io_uring_initialized_(false),
      io_uring_queue_depth_(options_.io_uring_queue_depth == 0 ? 64 : options_.io_uring_queue_depth)
#endif
{
#ifdef _WIN32
  direct_file_handle_ = INVALID_HANDLE_VALUE;
#endif
#if !defined(CORE_ENGINE_HAS_IO_URING)
  options_.enable_io_uring = false;
  options_.io_uring_queue_depth = 0;
#else
  if (options_.io_uring_queue_depth == 0) {
    options_.io_uring_queue_depth = 64;
    io_uring_queue_depth_ = 64;
  }
#endif
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

  ShutdownIoUringLocked();

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

  if (std::fseek(file_handle_, 0, SEEK_END) != 0) {
    std::fclose(file_handle_);
    file_handle_ = nullptr;
    return Status::IoError("Failed to seek file end");
  }

  const long file_size = std::ftell(file_handle_);
  if (file_size < 0) {
    std::fclose(file_handle_);
    file_handle_ = nullptr;
    return Status::IoError("Failed to determine file size");
  }

  std::fseek(file_handle_, 0, SEEK_SET);

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
  std::array<PageReadRequest, 1> requests{{PageReadRequest{page_id, page}}};
  return ReadPagesBatch(requests);
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
  std::array<PageWriteRequest, 1> requests{{PageWriteRequest{page_id, page}}};
  return WritePagesBatch(requests);
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
      tasks.push_back({IoTask::Type::kRead, request.page_id, request.page, nullptr});
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
    return Status::InvalidArgument("Requested range is too large");
  }

  const std::int64_t offset = PageIdToOffset(first_page_id);
  const std::size_t total_bytes = page_count * kPageSize;
  auto status = ReadRawLocked(offset, buffer, total_bytes);
  if (status.ok()) {
    stats_.total_reads += page_count;
  }
  return status;
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
      tasks.push_back({IoTask::Type::kWrite, request.page_id, nullptr, request.page});
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

PageId DiskManager::AllocatePage() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!is_open_) {
    return kInvalidPageId;
  }

  // Page 0 is reserved, so first valid page is 1
  // New pages are appended to the end of the file
  const PageId new_page_id = num_pages_ + 1;

  const std::int64_t offset = PageIdToOffset(new_page_id);

  Page empty_page;
  empty_page.Reset(new_page_id);
  empty_page.UpdateChecksum();

  const Status status = WriteRawLocked(offset, empty_page.GetRawPage(), kPageSize);
  if (!status.ok()) {
    Log(LogLevel::kError, "Failed to allocate page: " + std::string(status.message()));
    return kInvalidPageId;
  }

  // Update num_pages_ to reflect the new highest page
  num_pages_ = new_page_id;
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
      const DWORD chunk =
          static_cast<DWORD>(std::min<std::size_t>(remaining, std::numeric_limits<DWORD>::max()));
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
  if (fseeko(file_handle_, offset, SEEK_SET) != 0) {
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
      const DWORD chunk =
          static_cast<DWORD>(std::min<std::size_t>(remaining, std::numeric_limits<DWORD>::max()));
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
    if (fseeko(file_handle_, offset, SEEK_SET) != 0) {
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
        io_uring_prep_read(sqe, file_descriptor_, task->read_page->GetRawPage(), kPageSize, offset);
      } else {
        io_uring_prep_write(sqe, file_descriptor_, task->write_page->GetRawPage(), kPageSize,
                            offset);
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
#endif

} // namespace core_engine
