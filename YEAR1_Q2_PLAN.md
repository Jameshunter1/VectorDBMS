# Year 1 Q2: BufferPoolManager Implementation Plan

## Milestone Overview

**Goal**: Implement a production-grade buffer pool manager for page caching  
**Duration**: Q2 2026 (3 months)  
**Branch**: `feature/buffer-pool-manager`

## Architecture Components

### 1. BufferPoolManager Core

**File**: `src/include/core_engine/storage/buffer_pool.hpp`

```cpp
class BufferPoolManager {
 public:
  explicit BufferPoolManager(size_t pool_size, DiskManager* disk_manager);
  
  // Core operations
  Page* FetchPage(PageId page_id);           // Get page from pool or disk
  Status UnpinPage(PageId page_id, bool is_dirty);  // Release page reference
  Status FlushPage(PageId page_id);          // Write page to disk
  Status FlushAllPages();                    // Checkpoint operation
  PageId NewPage();                          // Allocate new page
  Status DeletePage(PageId page_id);         // Mark page as deleted
  
 private:
  std::vector<Page> pages_;                  // Frame array (pool_size pages)
  std::unordered_map<PageId, frame_id_t> page_table_;  // PageId → frame index
  std::list<frame_id_t> free_list_;          // Available frames
  Replacer* replacer_;                       // Eviction policy (stub for Q2)
  DiskManager* disk_manager_;                // Disk I/O
  
  std::mutex latch_;                         // Coarse-grained lock for Q2
  // Year 1 Q3: Fine-grained latching per frame
};
```

### 2. Page Pinning Semantics

**Concept**: Pages must be "pinned" while in use to prevent eviction

```cpp
// Manual pinning (error-prone)
Page* page = buffer_pool->FetchPage(page_id);  // pin_count = 1
// ... use page ...
buffer_pool->UnpinPage(page_id, false);  // pin_count = 0

// RAII pinning (recommended)
class PageGuard {
 public:
  PageGuard(BufferPoolManager* bpm, PageId page_id);
  ~PageGuard() { bpm_->UnpinPage(page_id_, is_dirty_); }
  
  Page* GetPage() { return page_; }
  void MarkDirty() { is_dirty_ = true; }
  
 private:
  BufferPoolManager* bpm_;
  PageId page_id_;
  Page* page_;
  bool is_dirty_ = false;
};
```

### 3. Page Table

**Purpose**: Fast lookup from page_id to frame index

```cpp
// Hash table: PageId → frame_id_t
std::unordered_map<PageId, frame_id_t> page_table_;

// Thread safety: Protected by buffer pool latch
// Year 1 Q3: Separate latch for page table
```

### 4. Free List

**Purpose**: Track available frames

```cpp
std::list<frame_id_t> free_list_;  // Frames not holding pages

// Algorithm:
// 1. FetchPage: Check page_table_
//    - Hit: Increment pin_count, return frame
//    - Miss: Get frame from free_list_ or evict victim
// 2. Load page from disk into frame
// 3. Update page_table_
// 4. Return page pointer
```

### 5. Eviction Policy (Stub)

**Year 1 Q2**: Simple FIFO eviction (for testing)  
**Year 1 Q3**: LRU-K replacer (full implementation)

```cpp
class Replacer {
 public:
  virtual bool Victim(frame_id_t* frame_id) = 0;  // Select victim for eviction
  virtual void Pin(frame_id_t frame_id) = 0;      // Mark as in-use
  virtual void Unpin(frame_id_t frame_id) = 0;    // Mark as evictable
};

// Q2: Stub implementation (simple FIFO)
class FIFOReplacer : public Replacer { /* ... */ };

// Q3: Full implementation
class LRUKReplacer : public Replacer { /* K=2 history tracking */ };
```

## Implementation Tasks

### Week 1-2: Core Structure
- [ ] Create `buffer_pool.hpp` header with class declaration
- [ ] Implement constructor/destructor
- [ ] Implement `FetchPage()` with page table lookup
- [ ] Implement `UnpinPage()` with pin count management
- [ ] Write unit tests for basic fetch/unpin operations

### Week 3-4: Page Lifecycle
- [ ] Implement `NewPage()` for page allocation
- [ ] Implement `DeletePage()` for page deletion
- [ ] Implement dirty page tracking
- [ ] Write tests for page creation/deletion

### Week 5-6: Flushing
- [ ] Implement `FlushPage()` for single-page write
- [ ] Implement `FlushAllPages()` for checkpoint
- [ ] Add background flush thread (optional for Q2)
- [ ] Write tests for flush operations

### Week 7-8: Eviction (Simple)
- [ ] Implement `FIFOReplacer` stub
- [ ] Integrate eviction into `FetchPage()` miss path
- [ ] Test eviction correctness (page written before eviction)
- [ ] Write stress tests (pool size < working set)

### Week 9-10: Thread Safety
- [ ] Add coarse-grained latch to BufferPoolManager
- [ ] Test concurrent fetch/unpin operations
- [ ] Benchmark single-threaded vs multi-threaded
- [ ] Profile lock contention (baseline for Q3 optimization)

### Week 11-12: Integration & Polish
- [ ] Update `Engine` to use `BufferPoolManager`
- [ ] Fix all existing tests to work with buffer pool
- [ ] Write comprehensive integration tests
- [ ] Update documentation and benchmarks
- [ ] Merge to `develop`, create PR to `master`

## Testing Strategy

### Unit Tests
```cpp
TEST_CASE("BufferPoolManager FetchPage hits cache") {
  DiskManager disk("./test.db");
  BufferPoolManager bpm(10, &disk);  // 10-page pool
  
  // First fetch: disk read
  Page* page1 = bpm.FetchPage(1);
  REQUIRE(page1 != nullptr);
  REQUIRE(page1->GetPageId() == 1);
  
  // Second fetch: cache hit (no disk I/O)
  Page* page2 = bpm.FetchPage(1);
  REQUIRE(page2 == page1);  // Same frame
  
  bpm.UnpinPage(1, false);
  bpm.UnpinPage(1, false);
}

TEST_CASE("BufferPoolManager evicts unpinned pages when full") {
  DiskManager disk("./test.db");
  BufferPoolManager bpm(2, &disk);  // 2-page pool
  
  // Fill pool
  Page* page1 = bpm.FetchPage(1);
  Page* page2 = bpm.FetchPage(2);
  bpm.UnpinPage(1, false);  // page1 now evictable
  bpm.UnpinPage(2, false);  // page2 now evictable
  
  // Trigger eviction
  Page* page3 = bpm.FetchPage(3);  // Should evict page1 or page2
  REQUIRE(page3 != nullptr);
}
```

### Integration Tests
```cpp
TEST_CASE("Engine uses BufferPoolManager for all operations") {
  Engine engine;
  engine.Open("./test_db");
  
  // Write 1000 key-value pairs
  for (int i = 0; i < 1000; i++) {
    engine.Put("key" + std::to_string(i), "value" + std::to_string(i));
  }
  
  // Verify all reads (should hit buffer pool)
  for (int i = 0; i < 1000; i++) {
    auto value = engine.Get("key" + std::to_string(i));
    REQUIRE(value.has_value());
  }
  
  // Check buffer pool stats
  auto stats = engine.GetStats();
  REQUIRE(stats.buffer_pool_hits > 0);
}
```

### Performance Tests
```cpp
BENCHMARK("BufferPool sequential reads") {
  // Measure throughput for sequential page access
  // Baseline: ~1M ops/sec (cache hit)
}

BENCHMARK("BufferPool random reads") {
  // Measure throughput for random page access
  // Expected: Degrades with low buffer pool hit rate
}
```

## Success Criteria

- [ ] All unit tests pass (>95% code coverage)
- [ ] Integration tests pass with existing Engine API
- [ ] No memory leaks (Valgrind/AddressSanitizer)
- [ ] Thread-safe concurrent access (ThreadSanitizer)
- [ ] Performance: >500K pages/sec for cache hits
- [ ] Documentation: Complete API docs and examples

## Dependencies

**Before Starting Q2**:
- ✅ Year 1 Q1 complete (Page, DiskManager)
- ✅ Test infrastructure (Catch2)
- ✅ Benchmark infrastructure (Google Benchmark)

**Blocks Q3**:
- LRU-K Replacer depends on BufferPoolManager interface

## Key Design Decisions

### Decision 1: Coarse vs Fine-Grained Locking
**Q2 Choice**: Coarse-grained (single latch for entire buffer pool)  
**Rationale**: Simpler implementation, get correctness first  
**Q3 Upgrade**: Fine-grained (per-frame latches + page table latch)

### Decision 2: Background Flusher Thread
**Q2 Choice**: Optional (implement if time permits)  
**Rationale**: Not critical for correctness, nice-to-have for performance  
**Alternative**: Explicit flush on eviction (simpler)

### Decision 3: Page Guard RAII Wrapper
**Q2 Choice**: Implement basic version  
**Rationale**: Prevents pin count leaks, essential for robust code  
**Future**: Read/write guard variants (shared vs exclusive latches)

## Resources

**Reference Implementations**:
- CMU 15-445 Database Systems (Andy Pavlo)
- PostgreSQL buffer manager
- RocksDB block cache

**Papers**:
- "The Five-Minute Rule" (Jim Gray & Franco Putzolu, 1987)
- "LRU-K: An O(1) Page Replacement Algorithm" (O'Neil et al, 1993)

---

**Start Date**: February 2026  
**Target Completion**: April 2026  
**Branch**: `feature/buffer-pool-manager`
