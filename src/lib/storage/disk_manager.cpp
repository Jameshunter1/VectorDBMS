#include <core_engine/common/logger.hpp>
#include <core_engine/storage/disk_manager.hpp>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <filesystem>
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
      options_(std::move(options)), file_descriptor_(-1)
#if defined(CORE_ENGINE_HAS_IO_URING)
      ,
      io_uring_initialized_(false),
      io_uring_queue_depth_(options_.io_uring_queue_depth == 0 ? 64 : options_.io_uring_queue_depth)
#endif
{
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

  // Open file for read/write, create if doesn't exist
  // Windows: FILE_FLAG_NO_BUFFERING for O_DIRECT equivalent
  // POSIX: Would use open() with O_DIRECT flag

#ifdef _WIN32
  // Windows implementation using CreateFile for FILE_FLAG_NO_BUFFERING
  // Note: For simplicity in this milestone, we use standard fopen
  // Production code should use CreateFile with FILE_FLAG_NO_BUFFERING
  // and ReadFile/WriteFile with aligned buffers

  // Check if file exists to determine initial page count
  bool file_exists = fs::exists(db_file_);

  // Use _wfopen_s for security compliance
  errno_t err = _wfopen_s(&file_handle_, db_file_.wstring().c_str(), L"rb+");
  if (err != 0 && !file_exists) {
    // File doesn't exist, create it
    err = _wfopen_s(&file_handle_, db_file_.wstring().c_str(), L"wb+");
  }

#else
  // POSIX implementation
  bool file_exists = fs::exists(db_file_);

  file_handle_ = std::fopen(db_file_.c_str(), "rb+");
  if (!file_handle_ && !file_exists) {
    // File doesn't exist, create it
    file_handle_ = std::fopen(db_file_.c_str(), "wb+");
  }
#endif

  if (!file_handle_) {
    return Status::IoError("Failed to open file: " + db_file_.string());
  }

  // Determine file size and calculate number of pages
  std::fseek(file_handle_, 0, SEEK_END);
  const long file_size = std::ftell(file_handle_);
  std::fseek(file_handle_, 0, SEEK_SET);

  if (file_size < 0) {
    std::fclose(file_handle_);
    file_handle_ = nullptr;
    return Status::IoError("Failed to determine file size");
  }

  // File size must be multiple of page size
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

  is_open_ = true;

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

  Log(LogLevel::kDebug,
      "DiskManager opened: " + db_file_.string() + " (" + std::to_string(num_pages_) + " pages)");

  return Status::Ok();
}

void DiskManager::Close() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!is_open_) {
    return;
  }

  ShutdownIoUringLocked();

  if (file_handle_) {
    std::fflush(file_handle_); // Flush any buffered writes
    std::fclose(file_handle_);
    file_handle_ = nullptr;
  }

  file_descriptor_ = -1;
  is_open_ = false;

  Log(LogLevel::kDebug, "DiskManager closed: " + db_file_.string());
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
    if (file_handle_ && std::fflush(file_handle_) != 0) {
      return Status::IoError("Failed to flush batch write to disk");
    }
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
  // Calculate file offset
  const std::int64_t offset = PageIdToOffset(page_id);

  // Seek to page position
  if (std::fseek(file_handle_, static_cast<long>(offset), SEEK_SET) != 0) {
    return Status::IoError("Failed to seek to page " + std::to_string(page_id));
  }

  const std::size_t bytes_read = std::fread(page->GetRawPage(), 1, kPageSize, file_handle_);

  if (bytes_read != kPageSize) {
    ++stats_.checksum_failures;
    return Status::IoError("Failed to read page " + std::to_string(page_id) + " (read " +
                           std::to_string(bytes_read) + " bytes)");
  }

  auto status = ValidateReadResult(page_id, page);
  if (!status.ok()) {
    return status;
  }

  ++stats_.total_reads;
  return Status::Ok();
}

Status DiskManager::PerformSingleWriteLocked(PageId page_id, const Page* page) {
  const std::int64_t offset = PageIdToOffset(page_id);

  if (std::fseek(file_handle_, static_cast<long>(offset), SEEK_SET) != 0) {
    return Status::IoError("Failed to seek to page " + std::to_string(page_id));
  }

  const std::size_t bytes_written = std::fwrite(page->GetRawPage(), 1, kPageSize, file_handle_);

  if (bytes_written != kPageSize) {
    return Status::IoError("Failed to write page " + std::to_string(page_id) + " (wrote " +
                           std::to_string(bytes_written) + " bytes)");
  }

  if (std::fflush(file_handle_) != 0) {
    return Status::IoError("Failed to flush page " + std::to_string(page_id) + " to disk");
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

  // Grow file by one page
  // Seek to end and write a zero page
  const std::int64_t offset = PageIdToOffset(new_page_id);

  if (std::fseek(file_handle_, static_cast<long>(offset), SEEK_SET) != 0) {
    Log(LogLevel::kError, "Failed to seek for page allocation");
    return kInvalidPageId;
  }

  // Write empty page (all zeros)
  Page empty_page;
  empty_page.Reset(new_page_id);
  empty_page.UpdateChecksum();

  const std::size_t bytes_written =
      std::fwrite(empty_page.GetRawPage(), 1, kPageSize, file_handle_);

  if (bytes_written != kPageSize) {
    Log(LogLevel::kError, "Failed to allocate page");
    return kInvalidPageId;
  }

  // CRITICAL: Flush immediately so allocated page is visible to reads
  if (std::fflush(file_handle_) != 0) {
    Log(LogLevel::kError, "Failed to flush allocated page");
    return kInvalidPageId;
  }

  // Update num_pages_ to reflect the new highest page
  num_pages_ = new_page_id;
  ++stats_.total_allocations;

  return new_page_id;
}

Status DiskManager::Sync() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!is_open_ || !file_handle_) {
    return Status::Internal("DiskManager not open");
  }

  // Flush stdio buffers
  if (std::fflush(file_handle_) != 0) {
    return Status::IoError("fflush failed");
  }

#ifdef _WIN32
  // Windows: FlushFileBuffers to ensure data is on disk
  const int fd = _fileno(file_handle_);
  if (fd < 0) {
    return Status::IoError("Invalid file descriptor");
  }
  HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
  if (handle == INVALID_HANDLE_VALUE) {
    return Status::IoError("Failed to get file handle");
  }
  if (!FlushFileBuffers(handle)) {
    return Status::IoError("FlushFileBuffers failed");
  }
#else
  // POSIX: fsync to ensure data is on disk
  const int fd = fileno(file_handle_);
  if (fd < 0) {
    return Status::IoError("Invalid file descriptor");
  }
  if (fsync(fd) != 0) {
    return Status::IoError("fsync failed");
  }
#endif

  return Status::Ok();
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
