#include <core_engine/common/logger.hpp>
#include <core_engine/storage/disk_manager.hpp>

#include <cstring>
#include <filesystem>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace core_engine {

namespace fs = std::filesystem;

DiskManager::DiskManager(std::filesystem::path db_file)
    : db_file_(std::move(db_file)), file_handle_(nullptr), is_open_(false), num_pages_(0) {}

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
  is_open_ = true;

  Log(LogLevel::kDebug,
      "DiskManager opened: " + db_file_.string() + " (" + std::to_string(num_pages_) + " pages)");

  return Status::Ok();
}

void DiskManager::Close() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!is_open_) {
    return;
  }

  if (file_handle_) {
    std::fflush(file_handle_); // Flush any buffered writes
    std::fclose(file_handle_);
    file_handle_ = nullptr;
  }

  is_open_ = false;

  Log(LogLevel::kDebug, "DiskManager closed: " + db_file_.string());
}

Status DiskManager::ReadPage(PageId page_id, Page* page) {
  if (!page) {
    return Status::InvalidArgument("Page pointer is null");
  }

  std::lock_guard<std::mutex> lock(mutex_);

  if (!is_open_) {
    return Status::Internal("DiskManager not open");
  }

  if (!IsValidPageId(page_id)) {
    return Status::InvalidArgument("Invalid page_id: " + std::to_string(page_id));
  }

  // Calculate file offset
  const std::int64_t offset = PageIdToOffset(page_id);

  // Seek to page position
  if (std::fseek(file_handle_, static_cast<long>(offset), SEEK_SET) != 0) {
    return Status::IoError("Failed to seek to page " + std::to_string(page_id));
  }

  // Read 4 KB page
  const std::size_t bytes_read = std::fread(page->GetRawPage(), 1, kPageSize, file_handle_);

  if (bytes_read != kPageSize) {
    ++stats_.checksum_failures; // Count as I/O error
    return Status::IoError("Failed to read page " + std::to_string(page_id) + " (read " +
                           std::to_string(bytes_read) + " bytes)");
  }

  // Verify checksum
  if (!page->VerifyChecksum()) {
    ++stats_.checksum_failures;
    return Status::Corruption("Checksum mismatch for page " + std::to_string(page_id));
  }

  // Verify page_id matches (sanity check)
  if (page->GetPageId() != page_id && page->GetPageId() != kInvalidPageId) {
    return Status::Corruption("Page ID mismatch: expected " + std::to_string(page_id) + ", got " +
                              std::to_string(page->GetPageId()));
  }

  ++stats_.total_reads;
  return Status::Ok();
}

Status DiskManager::WritePage(PageId page_id, const Page* page) {
  if (!page) {
    return Status::InvalidArgument("Page pointer is null");
  }

  std::lock_guard<std::mutex> lock(mutex_);

  if (!is_open_) {
    return Status::Internal("DiskManager not open");
  }

  if (!IsValidPageId(page_id)) {
    return Status::InvalidArgument("Invalid page_id: " + std::to_string(page_id));
  }

  // Verify checksum was updated (sanity check)
  if (!page->VerifyChecksum()) {
    return Status::InvalidArgument(
        "Page checksum not valid - call UpdateChecksum() before WritePage");
  }

  // Calculate file offset
  const std::int64_t offset = PageIdToOffset(page_id);

  // Seek to page position
  if (std::fseek(file_handle_, static_cast<long>(offset), SEEK_SET) != 0) {
    return Status::IoError("Failed to seek to page " + std::to_string(page_id));
  }

  // Write 4 KB page
  const std::size_t bytes_written = std::fwrite(page->GetRawPage(), 1, kPageSize, file_handle_);

  if (bytes_written != kPageSize) {
    return Status::IoError("Failed to write page " + std::to_string(page_id) + " (wrote " +
                           std::to_string(bytes_written) + " bytes)");
  }

  // CRITICAL: Flush to disk immediately on Windows/POSIX
  // Without this, reads may fail as data is still in buffer
  if (std::fflush(file_handle_) != 0) {
    return Status::IoError("Failed to flush page " + std::to_string(page_id) + " to disk");
  }

  ++stats_.total_writes;
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

} // namespace core_engine
