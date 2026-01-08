#pragma once

// core_engine/storage/disk_manager.hpp
//
// Purpose:
// - Raw block I/O layer with O_DIRECT for kernel bypass
// - Manages page allocation, atomic writes, and file-level operations
// - Year 1 Q1 milestone: Production-grade disk I/O foundation
//
// Design decisions:
// - O_DIRECT (Windows: FILE_FLAG_NO_BUFFERING) bypasses OS page cache
//   This gives database full control over caching (via BufferPoolManager in Q2)
// - All I/O must be aligned to sector size (typically 512 bytes or 4 KB)
// - Atomic page writes (torn page detection via LSN + checksum)
// - Thread-safe (can be called from multiple buffer pool threads)

#include <core_engine/common/status.hpp>
#include <core_engine/storage/page.hpp>

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <span>
#include <vector>

#if defined(CORE_ENGINE_HAS_IO_URING)
#include <liburing.h>
#endif

namespace core_engine {

// DiskManager: Raw page-level I/O with O_DIRECT
//
// Responsibilities:
// - Open/close database files
// - Read/write 4 KB pages at specific offsets
// - Allocate new pages (grow file size)
// - fsync for durability
//
// NOT responsible for:
// - Caching (that's BufferPoolManager's job in Q2)
// - Concurrency control (higher layers use latches)
// - Crash recovery (LogManager's job in Q4)
//
// Thread safety:
// - All public methods are thread-safe
// - Uses mutex for file operations (coarse-grained for simplicity)
// - Future: lock-free queues, io_uring batch submission (Year 2)
class DiskManager {
public:
  struct Options {
#if defined(CORE_ENGINE_HAS_IO_URING)
    bool enable_io_uring = true;
    std::uint32_t io_uring_queue_depth = 64;
#else
    bool enable_io_uring = false;
    std::uint32_t io_uring_queue_depth = 0;
#endif
  };

  struct PageReadRequest {
    PageId page_id;
    Page* page;
  };

  struct PageWriteRequest {
    PageId page_id;
    const Page* page;
  };

  // Create DiskManager for a specific database file
  explicit DiskManager(std::filesystem::path db_file, Options options = {});
  ~DiskManager();

  // Disable copy (manages OS file handle)
  DiskManager(const DiskManager&) = delete;
  DiskManager& operator=(const DiskManager&) = delete;

  // ========== Lifecycle ==========

  // Open or create the database file
  // Creates file if it doesn't exist, sets up O_DIRECT flags
  Status Open();

  // Close the database file
  // Ensures all pending writes are flushed before closing
  void Close();

  bool IsOpen() const {
    return is_open_;
  }

  // ========== Page I/O ==========

  // Read a page from disk into provided Page object
  // page_id: Which page to read (0-indexed, page 0 is reserved)
  // page: Output buffer (must be 4 KB aligned)
  // Returns: Status::Ok() on success, Status::IoError() on failure
  //
  // Postconditions on success:
  // - Page data matches disk content
  // - Checksum is verified (Status::Corruption if mismatch)
  // - LSN is consistent
  Status ReadPage(PageId page_id, Page* page);

  // Batch read multiple pages. On Linux, requests are issued through io_uring
  // when enabled; other platforms fall back to sequential reads.
  Status ReadPagesBatch(std::span<PageReadRequest> requests);

  // Write a page from memory to disk
  // page_id: Which page to write (0-indexed)
  // page: Source buffer (must be 4 KB aligned, checksum updated)
  // Returns: Status::Ok() on success, Status::IoError() on failure
  //
  // Preconditions:
  // - page->UpdateChecksum() must be called before WritePage
  // - page->GetPageId() should match page_id
  //
  // Atomicity:
  // - Single 4 KB write is atomic on modern hardware
  // - Torn page detection via checksum + LSN
  Status WritePage(PageId page_id, const Page* page);

  // Batch write multiple pages leveraging io_uring when available.
  Status WritePagesBatch(std::span<PageWriteRequest> requests);

  // Allocate a new page (grows the file)
  // Returns: PageId of the newly allocated page
  //
  // Thread safety:
  // - Atomic (uses mutex to prevent race conditions)
  // - Multiple threads can call concurrently
  PageId AllocatePage();

  // Get current number of pages in the file
  PageId GetNumPages() const {
    return num_pages_;
  }

  // ========== Durability ==========

  // Force all pending writes to physical disk
  // This is expensive (blocks until NVMe confirms persistence)
  // Use sparingly: after WAL flushes, at checkpoints
  Status Sync();

  // ========== Statistics (debugging/monitoring) ==========

  struct Stats {
    std::uint64_t total_reads = 0;       // Total ReadPage calls
    std::uint64_t total_writes = 0;      // Total WritePage calls
    std::uint64_t total_allocations = 0; // Total AllocatePage calls
    std::uint64_t checksum_failures = 0; // Failed checksum verifications
  };

  Stats GetStats() const;

private:
  // Helper: Check if page_id is valid (not reserved, within file bounds)
  bool IsValidPageId(PageId page_id) const;

  // Helper: Convert page_id to byte offset in file
  std::int64_t PageIdToOffset(PageId page_id) const;

  Status PerformSingleReadLocked(PageId page_id, Page* page);
  Status PerformSingleWriteLocked(PageId page_id, const Page* page);
  Status ProcessReadRequestsLocked(std::span<PageReadRequest> requests);
  Status ProcessWriteRequestsLocked(std::span<PageWriteRequest> requests);
  Status ValidateReadResult(PageId page_id, Page* page);

#if defined(CORE_ENGINE_HAS_IO_URING)
  struct IoTask {
    enum class Type { kRead, kWrite };
    Type type;
    PageId page_id;
    Page* read_page;
    const Page* write_page;
  };

  Status InitializeIoUringLocked();
  void ShutdownIoUringLocked();
  Status SubmitIoTasksLocked(std::span<IoTask> tasks);
#else
  Status InitializeIoUringLocked() {
    return Status::Ok();
  }
  void ShutdownIoUringLocked() {}
#endif

  std::filesystem::path db_file_; // Path to database file
  FILE* file_handle_;             // OS file handle (using C stdio for simplicity)
  bool is_open_;                  // Is file currently open?
  PageId num_pages_;              // Current number of pages in file
  Options options_;
  int file_descriptor_;

#if defined(CORE_ENGINE_HAS_IO_URING)
  bool io_uring_initialized_;
  std::uint32_t io_uring_queue_depth_;
  io_uring io_ring_;
#endif

  mutable std::mutex mutex_; // Coarse-grained lock for thread safety
  Stats stats_;              // I/O statistics
};

} // namespace core_engine
