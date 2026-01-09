#pragma once

#include <core_engine/common/status.hpp>
#include <core_engine/storage/page.hpp>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <span>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#if defined(CORE_ENGINE_HAS_IO_URING)
#include <liburing.h>
#endif

namespace core_engine {

class DiskManager {
public:
  struct Options {
    bool enable_io_uring;
    std::uint32_t io_uring_queue_depth;
    bool register_fixed_buffers;

    Options()
#if defined(CORE_ENGINE_HAS_IO_URING)
        : enable_io_uring(true), io_uring_queue_depth(64), register_fixed_buffers(true)
#else
        : enable_io_uring(false), io_uring_queue_depth(0), register_fixed_buffers(false)
#endif
    {
    }
  };

  struct PageReadRequest {
    PageId page_id;
    Page* page;
  };

  struct PageWriteRequest {
    PageId page_id;
    const Page* page;
  };

  explicit DiskManager(std::filesystem::path db_file, Options options = Options());
  ~DiskManager();

  DiskManager(const DiskManager&) = delete;
  DiskManager& operator=(const DiskManager&) = delete;

  // Lifecycle
  Status Open();
  void Close();
  bool IsOpen() const {
    return is_open_;
  }

  // Page I/O
  Status ReadPage(PageId page_id, Page* page);
  Status ReadPagesBatch(std::span<PageReadRequest> requests);
  Status WritePage(PageId page_id, const Page* page);
  Status WritePagesBatch(std::span<PageWriteRequest> requests);
  Status ReadContiguous(PageId first_page_id, void* buffer, std::size_t page_count);
  Status WriteContiguous(PageId first_page_id, const void* buffer, std::size_t page_count);

  // Allocation
  PageId AllocatePage();
  PageId GetNumPages() const {
    return num_pages_.load();
  }
  bool UsingDirectIO() const {
    return use_direct_io_;
  }

  // Durability
  Status Sync();

  // io_uring Zero-Copy
  Status RegisterFixedBuffers(std::span<Page> buffers);
  void UnregisterFixedBuffers();
  bool HasFixedBuffers() const {
#if defined(CORE_ENGINE_HAS_IO_URING)
    return fixed_buffers_registered_;
#else
    return false;
#endif
  }

  // Add this inside core_engine::DiskManager
  struct Stats {
    std::uint64_t total_reads;
    std::uint64_t total_writes;
    std::uint64_t total_allocations;
    std::uint64_t checksum_failures;
  };
  Stats GetStats() const;

private:
  bool IsValidPageId(PageId page_id) const;
  std::int64_t PageIdToOffset(PageId page_id) const;

  Status PerformSingleReadLocked(PageId page_id, Page* page);
  Status PerformSingleWriteLocked(PageId page_id, const Page* page);
  Status ProcessReadRequestsLocked(std::span<PageReadRequest> requests);
  Status ProcessWriteRequestsLocked(std::span<PageWriteRequest> requests);
  Status ValidateReadResult(PageId page_id, Page* page);
  Status OpenBufferedLocked();
  Status OpenDirectLocked();
  Status ReadRawLocked(std::int64_t offset, void* buffer, std::size_t size);
  Status WriteRawLocked(std::int64_t offset, const void* buffer, std::size_t size);
  Status FlushFileLocked();

#if defined(CORE_ENGINE_HAS_IO_URING)
  struct IoTask {
    enum class Type { kRead, kWrite } type;
    PageId page_id;
    Page* read_page;
    const Page* write_page;
    int buffer_index;
  };
  Status InitializeIoUringLocked();
  void ShutdownIoUringLocked();
  Status SubmitIoTasksLocked(std::span<IoTask> tasks);
  Status RegisterFixedBuffersLocked(std::span<Page> buffers);
  void UnregisterFixedBuffersLocked();
  int GetBufferIndex(const Page* page) const;
#endif

  std::filesystem::path db_file_;
  FILE* file_handle_ = nullptr;
  bool is_open_ = false;
  std::atomic<PageId> num_pages_{0};
  Options options_;
  int file_descriptor_ = -1;
  bool use_direct_io_ = false;

#ifdef _WIN32
  HANDLE direct_file_handle_ = INVALID_HANDLE_VALUE;
#endif

#if defined(CORE_ENGINE_HAS_IO_URING)
  bool io_uring_initialized_ = false;
  std::uint32_t io_uring_queue_depth_ = 0;
  io_uring io_ring_{};
  bool fixed_buffers_registered_ = false;
  Page* fixed_buffer_base_ = nullptr;
  std::size_t fixed_buffer_count_ = 0;
#endif

  mutable std::mutex mutex_;
  mutable Stats stats_;
};

} // namespace core_engine