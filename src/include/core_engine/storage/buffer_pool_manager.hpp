#pragma once

// core_engine/storage/buffer_pool_manager.hpp
//
// Purpose:
// - Main memory page cache with LRU-K eviction policy
// - Year 1 Q3 milestone: Advanced eviction with backward k-distance
//
// Design decisions:
// - Fixed-size pool (e.g., 1024 pages = 4 MB)
// - LRU-K replacement policy (evict page with max backward k-distance)
// - Pin count prevents eviction of in-use pages
// - Dirty bit tracking (only write dirty pages on eviction)
// - Thread-safe with page-level latches
//
// Architecture:
// - Page table: PageId -> frame_id (hash map for O(1) lookup)
// - Free list: Available frame slots
// - LRU-K replacer: Tracks last k access timestamps per frame
// - Replacer: Chooses victim page with maximum backward k-distance

#include <core_engine/common/status.hpp>
#include <core_engine/storage/disk_manager.hpp>
#include <core_engine/storage/page.hpp>

#include <chrono>
#include <list>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace core_engine {

// Forward declarations
class LRUKReplacer;

// BufferPoolManager: Page cache with LRU eviction
//
// Responsibilities:
// - Cache frequently accessed pages in memory
// - Pin/unpin pages to control eviction
// - Flush dirty pages to disk before eviction
// - Allocate new pages through DiskManager
//
// Usage pattern:
// 1. FetchPage(page_id) -> Pin page, increment ref count
// 2. Use page (read/write data)
// 3. UnpinPage(page_id, is_dirty) -> Decrement ref count, mark if modified
// 4. When pool is full, evict LRU unpinned page
//
// Thread safety:
// - Global latch protects page table and free list
// - Per-page rwlock for concurrent reads
// - Caller responsibility: hold appropriate latch while using page
class BufferPoolManager {
 public:
  // Create buffer pool with specified capacity
  // pool_size: Number of pages to cache (e.g., 1024 = 4 MB)
  // disk_manager: Backend storage for page I/O (not owned)
  BufferPoolManager(std::size_t pool_size, DiskManager* disk_manager);
  ~BufferPoolManager();

  // Disable copy/move (manages large memory pool)
  BufferPoolManager(const BufferPoolManager&) = delete;
  BufferPoolManager& operator=(const BufferPoolManager&) = delete;

  // ========== Page Operations ==========

  // Fetch a page from buffer pool (or load from disk)
  // page_id: Which page to fetch
  // Returns: Pointer to pinned page (caller must unpin when done)
  //
  // Behavior:
  // - If page in pool: increment pin count, return page
  // - If page not in pool: evict victim, load from disk, return page
  // - If pool full and no evictable page: return nullptr
  //
  // Thread safety: Page is pinned, safe to use after return
  Page* FetchPage(PageId page_id);

  // Unpin a page (caller done using it)
  // page_id: Which page to unpin
  // is_dirty: Has page been modified? (true = needs flush before eviction)
  // Returns: true if page was pinned, false otherwise
  //
  // Behavior:
  // - Decrement pin count
  // - If pin count reaches 0, page becomes evictable
  // - Set dirty bit if is_dirty=true
  //
  // IMPORTANT: Must unpin every fetched page, otherwise memory leak!
  bool UnpinPage(PageId page_id, bool is_dirty);

  // Flush a specific page to disk (write if dirty)
  // page_id: Which page to flush
  // Returns: true if page exists and flush succeeded
  //
  // Use cases:
  // - Checkpoint (flush all dirty pages)
  // - Explicit fsync for durability
  // - Clean eviction preparation
  bool FlushPage(PageId page_id);

  // Flush all pages to disk
  // Returns: true if all flushes succeeded
  //
  // Use cases:
  // - Database checkpoint
  // - Shutdown
  // - Recovery point
  bool FlushAllPages();

  // Allocate a new page (grow database file)
  // Returns: Pointer to new pinned page (caller must unpin when done)
  //          nullptr if allocation failed
  //
  // Behavior:
  // - Request new page from DiskManager
  // - Find free frame or evict victim
  // - Initialize page header
  // - Return pinned page
  Page* NewPage(PageId* page_id);

  // Delete a page (mark for removal)
  // page_id: Which page to delete
  // Returns: true if page exists and was deleted
  //
  // Behavior:
  // - Must be unpinned (pin_count == 0)
  // - Remove from page table
  // - Add frame to free list
  // - Future: mark page as deleted in disk (Q3 with WAL)
  bool DeletePage(PageId page_id);

  // ========== Statistics ==========

  struct Stats {
    std::size_t pool_size;        // Total frames in pool
    std::size_t pages_cached;     // Current pages in pool
    std::size_t cache_hits;       // FetchPage found in pool
    std::size_t cache_misses;     // FetchPage had to load from disk
    std::size_t pages_flushed;    // Dirty pages written to disk
    std::size_t pages_evicted;    // Pages evicted (LRU)
    std::size_t free_frames;      // Available frame slots
    std::size_t pinned_pages;     // Pages with pin_count > 0
    double hit_rate;              // cache_hits / (hits + misses)
  };

  Stats GetStats() const;

  // Get buffer pool size (max pages)
  std::size_t GetPoolSize() const { return pool_size_; }

 private:
  // ========== Internal Helpers ==========

  // Find a victim frame to evict (unpinned page with lowest LRU)
  // Returns: frame_id of victim, or -1 if no evictable page
  int FindVictimFrame();

  // Flush a page by frame_id (internal helper)
  bool FlushPageInternal(int frame_id);

  // ========== Data Members ==========

  const std::size_t pool_size_;                    // Max pages in pool
  Page* pages_;                                    // Array of pages [pool_size_]
  DiskManager* disk_manager_;                      // Backend storage (not owned)
  std::unique_ptr<LRUKReplacer> replacer_;         // LRU-K eviction policy

  std::unordered_map<PageId, int> page_table_;     // PageId -> frame_id
  std::list<int> free_list_;                       // Available frames

  mutable std::shared_mutex latch_;                // Protects page_table_ and free_list_

  // Statistics (atomic for lock-free updates)
  std::atomic<std::size_t> cache_hits_{0};
  std::atomic<std::size_t> cache_misses_{0};
  std::atomic<std::size_t> pages_flushed_{0};
  std::atomic<std::size_t> pages_evicted_{0};
};

// ============================================================================
// LRUKReplacer: Advanced eviction policy with backward k-distance
// ============================================================================

// LRUKReplacer tracks the last k accesses for each frame and evicts
// the frame with the maximum backward k-distance.
//
// Backward k-distance: Current time - timestamp of k-th most recent access
// - If frame accessed < k times: backward k-distance = +infinity
// - Otherwise: current_time - timestamp[k-th access]
//
// Operations:
// - RecordAccess(frame_id): Store timestamp of access
// - Evict(): Find and return frame with max backward k-distance
// - Pin(frame_id): Mark frame as non-evictable
// - Unpin(frame_id): Mark frame as evictable
// - Size(): Number of evictable frames
//
// Algorithm:
// - Maintain circular buffer of last k timestamps per frame
// - On eviction, scan all unpinned frames to find max backward k-distance
// - Frames with < k accesses are prioritized (infinite distance)
class LRUKReplacer {
 public:
  // k: Number of historical accesses to track (typically 2)
  // num_frames: Total number of frames in buffer pool
  LRUKReplacer(std::size_t k, std::size_t num_frames);

  // Record an access to a frame (store current timestamp)
  // frame_id: The frame being accessed
  void RecordAccess(int frame_id);

  // Find a victim frame to evict
  // Returns: frame_id with max backward k-distance, or -1 if none available
  //
  // Priority:
  // 1. Frames with < k accesses (infinite backward k-distance)
  // 2. Frames with oldest k-th access timestamp
  int Evict();

  // Pin a frame (make non-evictable)
  void Pin(int frame_id);

  // Unpin a frame (make evictable)
  void Unpin(int frame_id);

  // Get number of evictable frames
  std::size_t Size() const;

  // For backwards compatibility (aliased to Evict)
  int Victim() { return Evict(); }

 private:
  using Timestamp = std::chrono::steady_clock::time_point;

  struct FrameInfo {
    std::vector<Timestamp> history;  // Circular buffer of last k timestamps
    std::size_t history_size = 0;    // Number of valid entries in history
    std::size_t write_index = 0;     // Next position to write in circular buffer
    bool is_evictable = false;       // Can this frame be evicted?
  };

  // Calculate backward k-distance for a frame
  // Returns: Duration in milliseconds, or +infinity if < k accesses
  double GetBackwardKDistance(int frame_id, Timestamp current_time) const;

  std::size_t k_;                                   // Number of accesses to track
  std::size_t num_frames_;                          // Total frames tracked
  std::unordered_map<int, FrameInfo> frame_info_;   // Per-frame access history
  mutable std::mutex latch_;                        // Protects frame_info_
};

}  // namespace core_engine
