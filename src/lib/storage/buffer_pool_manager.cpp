// core_engine/storage/buffer_pool_manager.cpp
//
// Implementation of BufferPoolManager with LRU-K eviction policy
// Year 1 Q3: Advanced replacement using backward k-distance

#include <core_engine/storage/buffer_pool_manager.hpp>
#include <core_engine/common/logger.hpp>

#include <algorithm>
#include <cassert>
#include <limits>

namespace core_engine {

// ============================================================================
// BufferPoolManager Implementation
// ============================================================================

BufferPoolManager::BufferPoolManager(std::size_t pool_size, DiskManager* disk_manager)
    : pool_size_(pool_size),
      pages_(new Page[pool_size]),
      disk_manager_(disk_manager),
      replacer_(std::make_unique<LRUKReplacer>(2, pool_size)) {  // k=2 for LRU-2
  
  // Initialize free list with all frames
  for (std::size_t i = 0; i < pool_size_; ++i) {
    free_list_.push_back(static_cast<int>(i));
  }

  Log(LogLevel::kInfo, "BufferPoolManager initialized with " + 
      std::to_string(pool_size) + " pages (" + 
      std::to_string(pool_size * kPageSize / 1024) + " KB) using LRU-2 eviction");
}

BufferPoolManager::~BufferPoolManager() {
  // Flush all dirty pages before shutdown
  FlushAllPages();
  delete[] pages_;
  Log(LogLevel::kInfo, "BufferPoolManager destroyed");
}

Page* BufferPoolManager::FetchPage(PageId page_id) {
  std::unique_lock<std::shared_mutex> lock(latch_);

  // Check if page already in pool (cache hit)
  auto it = page_table_.find(page_id);
  if (it != page_table_.end()) {
    int frame_id = it->second;
    Page* page = &pages_[frame_id];
    
    // Increment pin count to prevent eviction
    page->IncrementPinCount();
    replacer_->Pin(frame_id);
    
    // Record access for LRU-K
    replacer_->RecordAccess(frame_id);
    
    ++cache_hits_;
    return page;
  }

  // Cache miss - need to load from disk
  ++cache_misses_;

  // Find a frame to use
  int frame_id;
  if (!free_list_.empty()) {
    // Use free frame
    frame_id = free_list_.front();
    free_list_.pop_front();
  } else {
    // No free frames - evict a victim
    frame_id = FindVictimFrame();
    if (frame_id == -1) {
      // No evictable pages (all pinned)
      Log(LogLevel::kWarn, "BufferPool full, all pages pinned");
      return nullptr;
    }

    // Flush victim if dirty
    Page* victim_page = &pages_[frame_id];
    if (victim_page->IsDirty()) {
      if (!FlushPageInternal(frame_id)) {
        Log(LogLevel::kError, "Failed to flush victim page");
        return nullptr;
      }
    }

    // Remove victim from page table
    PageId victim_page_id = victim_page->GetPageId();
    page_table_.erase(victim_page_id);
    ++pages_evicted_;
  }

  // Load page from disk
  Page* page = &pages_[frame_id];
  auto status = disk_manager_->ReadPage(page_id, page);
  
  if (!status.ok()) {
    // Read failed - return frame to free list
    free_list_.push_back(frame_id);
    Log(LogLevel::kError, "Failed to read page " + std::to_string(page_id) + 
        ": " + status.ToString());
    return nullptr;
  }

  // Verify checksum
  if (!page->VerifyChecksum()) {
    // Corrupted page - return frame to free list
    free_list_.push_back(frame_id);
    Log(LogLevel::kError, "Checksum verification failed for page " + 
        std::to_string(page_id));
    return nullptr;
  }

  // Update page table and pin page
  page_table_[page_id] = frame_id;
  page->IncrementPinCount();
  replacer_->Pin(frame_id);
  
  // Record access for LRU-K
  replacer_->RecordAccess(frame_id);

  return page;
}

bool BufferPoolManager::UnpinPage(PageId page_id, bool is_dirty) {
  std::shared_lock<std::shared_mutex> lock(latch_);

  auto it = page_table_.find(page_id);
  if (it == page_table_.end()) {
    // Page not in pool
    return false;
  }

  int frame_id = it->second;
  Page* page = &pages_[frame_id];

  // Check if page is pinned
  if (page->GetPinCount() == 0) {
    // Already unpinned
    return false;
  }

  // Decrement pin count
  page->DecrementPinCount();

  // Set dirty flag if requested
  if (is_dirty) {
    page->MarkDirty();
  }

  // If no more pins, make page evictable
  if (page->GetPinCount() == 0) {
    replacer_->Unpin(frame_id);
  }

  return true;
}

bool BufferPoolManager::FlushPage(PageId page_id) {
  std::shared_lock<std::shared_mutex> lock(latch_);

  auto it = page_table_.find(page_id);
  if (it == page_table_.end()) {
    // Page not in pool
    return false;
  }

  int frame_id = it->second;
  return FlushPageInternal(frame_id);
}

bool BufferPoolManager::FlushAllPages() {
  std::shared_lock<std::shared_mutex> lock(latch_);

  // Check if disk manager is still open (defensive check for shutdown)
  if (!disk_manager_ || !disk_manager_->IsOpen()) {
    Log(LogLevel::kWarn, "Cannot flush pages: DiskManager not open");
    return true; // Return true to avoid error logging during clean shutdown
  }

  bool all_success = true;
  for (const auto& [page_id, frame_id] : page_table_) {
    if (!FlushPageInternal(frame_id)) {
      all_success = false;
      Log(LogLevel::kError, "Failed to flush page " + std::to_string(page_id));
    }
  }

  // Sync all writes to disk
  if (all_success) {
    auto status = disk_manager_->Sync();
    if (!status.ok()) {
      Log(LogLevel::kError, "Failed to sync disk: " + status.ToString());
      all_success = false;
    }
  }

  return all_success;
}

Page* BufferPoolManager::NewPage(PageId* page_id) {
  std::unique_lock<std::shared_mutex> lock(latch_);

  // Allocate new page from disk manager
  PageId new_page_id = disk_manager_->AllocatePage();
  if (new_page_id == kInvalidPageId) {
    Log(LogLevel::kError, "Failed to allocate new page");
    return nullptr;
  }

  // Find a frame to use
  int frame_id;
  if (!free_list_.empty()) {
    // Use free frame
    frame_id = free_list_.front();
    free_list_.pop_front();
  } else {
    // No free frames - evict a victim
    frame_id = FindVictimFrame();
    if (frame_id == -1) {
      // No evictable pages - cannot create new page
      Log(LogLevel::kError, "Cannot create new page - buffer pool full");
      return nullptr;
    }

    // Flush victim if dirty
    Page* victim_page = &pages_[frame_id];
    if (victim_page->IsDirty()) {
      if (!FlushPageInternal(frame_id)) {
        Log(LogLevel::kError, "Failed to flush victim page");
        return nullptr;
      }
    }

    // Remove victim from page table
    PageId victim_page_id = victim_page->GetPageId();
    page_table_.erase(victim_page_id);
    ++pages_evicted_;
  }

  // Initialize new page
  Page* page = &pages_[frame_id];
  page->Reset(new_page_id);
  page->IncrementPinCount();
  page->MarkDirty();                      // New page needs to be written
  page->SetLSN(0);                        // TODO: Get from LogManager in Q4

  // CRITICAL: Update checksum so page can be read before flush
  // Without this, reading the page before it's flushed causes checksum mismatch
  page->UpdateChecksum();

  // Update page table
  page_table_[new_page_id] = frame_id;
  replacer_->Pin(frame_id);

  // Record access for LRU-K
  replacer_->RecordAccess(frame_id);

  // Return page_id to caller
  if (page_id != nullptr) {
    *page_id = new_page_id;
  }

  return page;
}

bool BufferPoolManager::DeletePage(PageId page_id) {
  std::unique_lock<std::shared_mutex> lock(latch_);

  auto it = page_table_.find(page_id);
  if (it == page_table_.end()) {
    // Page not in pool - nothing to delete
    return true;
  }

  int frame_id = it->second;
  Page* page = &pages_[frame_id];

  // Cannot delete pinned page
  if (page->GetPinCount() > 0) {
    Log(LogLevel::kWarn, "Cannot delete pinned page " + std::to_string(page_id));
    return false;
  }

  // Remove from page table
  page_table_.erase(it);

  // Reset page metadata
  page->Reset(kInvalidPageId);
  page->ClearDirty();

  // Add frame back to free list
  free_list_.push_back(frame_id);

  // Note: We can't delete from disk with DiskManager API
  // DiskManager doesn't have DeallocatePage method
  // Just mark the page as free in memory

  return true;
}

BufferPoolManager::Stats BufferPoolManager::GetStats() const {
  Stats stats;
  stats.cache_hits = cache_hits_.load();
  stats.cache_misses = cache_misses_.load();
  stats.pages_flushed = pages_flushed_.load();
  stats.pages_evicted = pages_evicted_.load();

  std::size_t total_requests = stats.cache_hits + stats.cache_misses;
  stats.hit_rate = (total_requests > 0) 
      ? static_cast<double>(stats.cache_hits) / total_requests 
      : 0.0;

  return stats;
}

// ========== Private Helpers ==========

int BufferPoolManager::FindVictimFrame() {
  // Try to find a victim using LRU-K replacer
  return replacer_->Evict();
}

bool BufferPoolManager::FlushPageInternal(int frame_id) {
  Page* page = &pages_[frame_id];

  // Skip if not dirty
  if (!page->IsDirty()) {
    return true;
  }

  // Check if disk manager is still open (defensive check)
  if (!disk_manager_ || !disk_manager_->IsOpen()) {
    Log(LogLevel::kWarn,
        "Cannot flush page " + std::to_string(page->GetPageId()) + ": DiskManager not open");
    return false;
  }

  // Update checksum before writing
  page->UpdateChecksum();

  // Write to disk
  auto status = disk_manager_->WritePage(page->GetPageId(), page);
  if (!status.ok()) {
    Log(LogLevel::kError, "Failed to write page " + 
        std::to_string(page->GetPageId()) + ": " + status.ToString());
    return false;
  }

  // Clear dirty bit
  page->ClearDirty();
  ++pages_flushed_;

  return true;
}

// ============================================================================
// LRUKReplacer Implementation
// ============================================================================

LRUKReplacer::LRUKReplacer(std::size_t k, std::size_t num_frames)
    : k_(k), num_frames_(num_frames) {
  // Pre-allocate history vectors for all frames
  for (std::size_t i = 0; i < num_frames; ++i) {
    frame_info_[static_cast<int>(i)].history.resize(k);
  }
}

void LRUKReplacer::RecordAccess(int frame_id) {
  std::lock_guard<std::mutex> lock(latch_);

  auto& info = frame_info_[frame_id];
  
  // Store current timestamp in circular buffer
  info.history[info.write_index] = std::chrono::steady_clock::now();
  info.write_index = (info.write_index + 1) % k_;
  
  if (info.history_size < k_) {
    ++info.history_size;
  }
}

int LRUKReplacer::Evict() {
  std::lock_guard<std::mutex> lock(latch_);

  int victim = -1;
  double max_distance = -1.0;
  auto current_time = std::chrono::steady_clock::now();

  // Scan all evictable frames to find maximum backward k-distance
  for (auto& [frame_id, info] : frame_info_) {
    if (!info.is_evictable) {
      continue;
    }

    double distance = GetBackwardKDistance(frame_id, current_time);
    
    if (distance > max_distance) {
      max_distance = distance;
      victim = frame_id;
    }
  }

  if (victim != -1) {
    frame_info_[victim].is_evictable = false;
  }

  return victim;
}

void LRUKReplacer::Pin(int frame_id) {
  std::lock_guard<std::mutex> lock(latch_);
  
  auto it = frame_info_.find(frame_id);
  if (it != frame_info_.end()) {
    it->second.is_evictable = false;
  }
}

void LRUKReplacer::Unpin(int frame_id) {
  std::lock_guard<std::mutex> lock(latch_);
  
  auto it = frame_info_.find(frame_id);
  if (it != frame_info_.end()) {
    it->second.is_evictable = true;
  }
}

std::size_t LRUKReplacer::Size() const {
  std::lock_guard<std::mutex> lock(latch_);
  
  std::size_t count = 0;
  for (const auto& [frame_id, info] : frame_info_) {
    if (info.is_evictable) {
      ++count;
    }
  }
  return count;
}

double LRUKReplacer::GetBackwardKDistance(int frame_id, Timestamp current_time) const {
  const auto& info = frame_info_.at(frame_id);

  // If fewer than k accesses, return +infinity (highest priority for eviction)
  if (info.history_size < k_) {
    return std::numeric_limits<double>::infinity();
  }

  // Find k-th most recent access (oldest in circular buffer)
  std::size_t kth_index = (info.write_index + k_ - info.history_size) % k_;
  auto kth_timestamp = info.history[kth_index];

  // Calculate backward k-distance (in milliseconds)
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      current_time - kth_timestamp);
  
  return static_cast<double>(duration.count());
}

}  // namespace core_engine
