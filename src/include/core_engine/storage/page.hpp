#pragma once

// core_engine/storage/page.hpp
//
// Purpose:
// - Define the fundamental 4 KB page structure for the storage engine.
// - Pages are the unit of I/O and caching (like PostgreSQL, MySQL InnoDB).
// - Cache-line aligned (64 bytes) for optimal CPU performance.
//
// Design decisions:
// - Fixed 4 KB size matches OS page size and NVMe block size
// - Header in first 64 bytes (single cache line) for hot metadata
// - LSN for recovery ordering (ARIES protocol)
// - Pin count for buffer pool management
// - CRC32 checksum for corruption detection

#include <core_engine/common/status.hpp>

#include <array>
#include <cstdint>
#include <cstring>

namespace core_engine {

// Page identifier type (32-bit allows 16 TB database with 4 KB pages)
using PageId = std::uint32_t;

// Log Sequence Number for recovery ordering
using LSN = std::uint64_t;

// Constants
constexpr PageId kInvalidPageId = 0;        // Page 0 is reserved/invalid by convention
constexpr std::size_t kPageSize = 4096;     // 4 KB pages (OS page size, NVMe friendly)
constexpr std::size_t kPageHeaderSize = 64; // Header fits in one cache line
constexpr std::size_t kPageDataSize = kPageSize - kPageHeaderSize; // 4032 bytes for data

// Page types for specialized handling
enum class PageType : std::uint8_t {
  kInvalid = 0,       // Uninitialized page
  kHeaderPage = 1,    // Database metadata (page 1)
  kBTreeInternal = 2, // B-tree internal node
  kBTreeLeaf = 3,     // B-tree leaf node
  kHeap = 4,          // Heap tuple storage
  kOverflow = 5,      // Large record overflow
  kFreeSpaceMap = 6,  // Tracks free space per page
  kVectorHNSW = 7,    // HNSW graph node storage
};

// Page header (64 bytes, cache-line aligned)
// All fields are explicitly sized for cross-platform compatibility
#pragma pack(push, 1)
struct PageHeader {
  PageId page_id;            // Unique identifier (offset 0, 4 bytes)
  LSN lsn;                   // Log sequence number for recovery (offset 4, 8 bytes)
  std::uint32_t pin_count;   // Reference count (0 = evictable) (offset 12, 4 bytes)
  std::uint32_t checksum;    // CRC32 of entire page (offset 16, 4 bytes)
  std::uint16_t free_space;  // Bytes available in data region (offset 20, 2 bytes)
  std::uint8_t is_dirty;     // Modified since last flush? (offset 22, 1 byte)
  PageType page_type;        // Type of page content (offset 23, 1 byte)
  std::uint8_t reserved[40]; // Reserved for future use (offset 24, pad to 64 bytes)

  PageHeader()
      : page_id(kInvalidPageId), lsn(0), pin_count(0), checksum(0), free_space(kPageDataSize),
        is_dirty(0), page_type(PageType::kInvalid) {
    std::memset(reserved, 0, sizeof(reserved));
  }
};
#pragma pack(pop)

static_assert(sizeof(PageHeader) == 64, "PageHeader must be exactly 64 bytes");

// Page: 4 KB storage unit
//
// Memory layout:
//   [0-63]:    PageHeader (cache-line aligned)
//   [64-4095]: Data region (4032 bytes)
//
// Thread safety:
//   - Pages are NOT thread-safe on their own
//   - BufferPoolManager provides latches for concurrent access
//   - Caller must hold appropriate latch before accessing page data
class alignas(4096) Page {
public:
  Page();
  ~Page() = default;

  // Disable copy (pages are large, should be moved or referenced)
  Page(const Page&) = delete;
  Page& operator=(const Page&) = delete;

  // Move is allowed (for returning from functions)
  Page(Page&&) noexcept = default;
  Page& operator=(Page&&) noexcept = default;

  // ========== Accessors ==========

  PageId GetPageId() const {
    return header_.page_id;
  }
  void SetPageId(PageId page_id) {
    header_.page_id = page_id;
  }

  LSN GetLSN() const {
    return header_.lsn;
  }
  void SetLSN(LSN lsn) {
    header_.lsn = lsn;
  }

  std::uint32_t GetPinCount() const {
    return header_.pin_count;
  }
  void IncrementPinCount() {
    ++header_.pin_count;
  }
  void DecrementPinCount() {
    if (header_.pin_count > 0)
      --header_.pin_count;
  }

  bool IsDirty() const {
    return header_.is_dirty != 0;
  }
  void MarkDirty() {
    header_.is_dirty = 1;
  }
  void ClearDirty() {
    header_.is_dirty = 0;
  }

  PageType GetPageType() const {
    return header_.page_type;
  }
  void SetPageType(PageType type) {
    header_.page_type = type;
  }

  std::uint16_t GetFreeSpace() const {
    return header_.free_space;
  }
  void SetFreeSpace(std::uint16_t free_space) {
    header_.free_space = free_space;
  }

  // ========== Data Access ==========

  // Get raw data pointer (caller must manage concurrency)
  char* GetData() {
    return data_.data();
  }
  const char* GetData() const {
    return data_.data();
  }

  // Get raw page pointer (includes header + data)
  char* GetRawPage() {
    return reinterpret_cast<char*>(this);
  }
  const char* GetRawPage() const {
    return reinterpret_cast<const char*>(this);
  }

  // ========== Checksum Operations ==========

  // Compute CRC32 checksum of entire page (excluding checksum field itself)
  std::uint32_t ComputeChecksum() const;

  // Update checksum field (call before writing to disk)
  void UpdateChecksum() {
    header_.checksum = ComputeChecksum();
  }

  // Verify checksum matches computed value (call after reading from disk)
  bool VerifyChecksum() const {
    return header_.checksum == ComputeChecksum();
  }

  // ========== Utilities ==========

  // Reset page to initial state (all zeros except page_id)
  void Reset(PageId page_id = kInvalidPageId);

  // Get size of page (always 4 KB)
  static constexpr std::size_t Size() {
    return kPageSize;
  }

  // Get data region size
  static constexpr std::size_t DataSize() {
    return kPageDataSize;
  }

private:
  PageHeader header_;                    // 64-byte header (cache-line aligned)
  std::array<char, kPageDataSize> data_; // 4032-byte data region
};

static_assert(sizeof(Page) == 4096, "Page must be exactly 4 KB");

} // namespace core_engine
