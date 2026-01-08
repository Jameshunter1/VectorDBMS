# Vectis — AI Coding Agent Instructions

## Project Overview

**C++20 page-oriented vector database engine** (Year 1 complete, production-ready). Hardware-aware design optimized for NVMe SSD, SIMD operations, and modern CPU architectures.

**Core Philosophy**: Explicit control over memory, deterministic behavior, and observable performance. Treat OS as coordinator, not cache manager.

**Key Achievement**: Recently fixed critical O(N) performance bug by adding O(1) hash map index (`std::unordered_map<std::string, PageId>`) in Engine class, eliminating linear page scans on every Put/Get/Delete.

## Repository Structure

```
VectorDBMS/                      # ⚠️ Run CMake from here, NOT from src/
├── src/                         # Source root (CMakeLists.txt entry point)
│   ├── include/core_engine/     # Public headers (subsystem-organized)
│   ├── lib/                     # Implementation (.cpp files)
│   ├── apps/                    # Executables: dbcli (CLI), dbweb (HTTP server)
│   └── CMakeLists.txt           # Main build config
├── tests/                       # Catch2 test suite
├── benchmarks/                  # Google Benchmark perf tests
├── build/                       # Build artifacts (gitignored)
├── docs/                        # Consolidated documentation
│   ├── DOCUMENTATION.md         # Complete API/build/deployment guide
│   └── CONTRIBUTING.md          # Development guidelines
└── docker-compose.yml           # Production stack (DB + Prometheus + Grafana)

```

## Critical Architecture Components

### Storage Layer (All Implemented ✅)

1. **Page** (4 KB): Cache-aligned fixed-size unit with LSN, pin_count, dirty flag, CRC32 checksum
   - Location: `src/include/core_engine/storage/page.hpp`
   - **Critical**: Page ID 0 is INVALID (`kInvalidPageId`). Always validate before use.

2. **DiskManager**: Raw block I/O with defensive lifecycle checks
   - **Recent Fix**: Added `IsOpen()` checks before all write operations to prevent "DiskManager not open" errors
   - Pattern: `if (!disk_manager_ || !disk_manager_->IsOpen()) { return; }`

3. **BufferPoolManager**: LRU-K page cache (4 MB default = 1024 pages × 4 KB)
   - **Thread-safe**: Fine-grained latches per frame, global latch for page table
   - **Critical Pattern**: ALWAYS pin before access, unpin when done (prevents eviction during use)
   - **Destruction Order**: BufferPoolManager → LogManager → DiskManager (prevents use-after-free)

4. **Engine**: High-level API with O(1) key-value index
   - **Critical Optimization** (Jan 2026): Added `key_to_page_` hash map to eliminate O(N) page scans
   - **Before**: Linear scan through all pages on every Get/Put/Delete
   - **After**: Direct page lookup with `key_to_page_[key]` → ~1000x faster on large databases
   - **Index Rebuild**: On `Open()`, scans existing pages to reconstruct in-memory index

5. **LogManager**: ARIES-style WAL (Write-Ahead Logging)
   - **Critical Rule**: WAL record MUST be flushed BEFORE page modification (LSN ordering)
   - Recovery: Analysis → Redo → Undo passes

### Vector Database Components

- **HNSWIndex**: O(log N) approximate nearest neighbor search (thread-safe with shared_mutex)
- **Vector**: SIMD-optimized distance calculations (AVX2/AVX-512 when available)
- **Config**: `DatabaseConfig::vector_dimension` must match ALL vectors (validated at insert)

### Monitoring & Security (Production-Ready)

- **Prometheus Metrics**: `/metrics` endpoint in dbweb app exports gauges/counters
- **AuthManager**: RBAC with session management (bcrypt password hashing)
- **RateLimiter**: Token bucket algorithm with per-client/per-endpoint limits
- **AuditLogger**: Security event logging (logins, unauthorized access, operations)

## Build System (Critical Knowledge)

### Multi-Config vs Single-Config Generators

**Windows (Visual Studio)** - Multi-config generator:
```powershell
cmake --preset windows-vs2022-x64-debug          # From repo root
cmake --build build/windows-vs2022-x64-debug --config Debug
ctest --test-dir build/windows-vs2022-x64-debug -C Debug --output-on-failure  # ⚠️ -C Debug REQUIRED
```

**Linux/macOS (Make or Ninja)** - Single-config generator:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure  # ⚠️ NO -C flag
```

**Key Difference**: Multi-config generators (VS, Xcode) require `-C Debug` in ctest, single-config don't.

### Docker Deployment

```bash
docker compose up -d                         # Starts DB + Prometheus + Grafana
curl http://localhost:8080/api/stats         # Database API
curl http://localhost:8080/metrics           # Prometheus metrics
# Grafana: http://localhost:3000 (admin/admin)
```


## Data Flow Patterns

### Write Path (Page-Based)
1. WAL first: `Engine::Put()` → `LogManager::AppendLog()` (group commit for batching)
2. Lookup page: `key_to_page_[key]` → page_id (O(1) hash map lookup)
3. Fetch page: `BufferPoolManager::FetchPage(page_id)` → Pin in memory
4. Modify in-page: Update page content
5. Mark dirty: Set dirty flag for later async flush
6. Unpin: Release pin, eligible for LRU-K eviction
7. Update index: `key_to_page_[key] = page_id` (maintains O(1) lookups)

### Read Path (Page-Based with O(1) Index)
1. Index lookup: `auto it = key_to_page_.find(key)` → O(1) hash lookup
2. If not found: Return `std::nullopt` immediately (no disk I/O)
3. If found: `BufferPoolManager::FetchPage(page_id)` checks page table
4. Cache hit: Increment pin_count, return frame pointer (~5-10 μs)
5. Cache miss: Evict victim via LRU-K, load from disk (~100-200 μs on NVMe)
6. Validate: Verify CRC32 checksum, check LSN consistency
7. Unpin: Release when done

### Recovery Path (ARIES-Style)
1. Load pages: Read `pages.db` from disk first
2. Build index: Scan pages to reconstruct `key_to_page_` map
3. WAL replay: Analysis → Redo → Undo passes
4. Checkpoint: Flush dirty pages, write checkpoint LSN


## Coding Conventions & Patterns

### Error Handling
- **Return `Status`, not exceptions**: All storage code returns `core_engine::Status`
- **Check before propagate**: Always verify `status.ok()` and return early on failure
- **WAL-before-modify pattern**:
  ```cpp
  auto status = log_manager_.AppendLog(log_record);
  if (!status.ok()) {
    return status;  // Do NOT modify page if WAL fails
  }
  page->MarkDirty();
  ```

### Memory Management
- **Explicit alignment**: 64-byte cache-line alignment for page headers
- **RAII for pins**: Always use pin/unpin pattern (prevents memory leaks)
- **Zero-copy**: Pass frame pointers directly, avoid buffer copies

### Naming Conventions
- **Namespace**: All code in `namespace core_engine`
- **Private members**: Trailing underscore (`buffer_pool_`, `disk_manager_`, `is_open_`)
- **Constants**: `kPascalCase` (e.g., `kPageSize`, `kInvalidPageId`)
- **Functions**: `PascalCase` (e.g., `FetchPage`, `FlushAllPages`)
- **Style**: LLVM (2-space indent, 100-char line limit)
- **Formatting**: Run `.\format.ps1` before committing (uses `.clang-format` in repo root)

### Thread Safety
- **BufferPoolManager**: Fine-grained latches per frame + global page table latch
- **HNSWIndex**: `shared_mutex` for concurrent reads, exclusive for writes
- **Engine**: Thread-safe at API level (internal synchronization)

### Performance Patterns
- **Avoid virtual dispatch in hot paths**: Use templates or function pointers
- **SIMD intrinsics**: AVX2/AVX-512 for vector distance calculations
- **Batch operations**: Group commit for WAL, batch page flushes
- **Prefetch**: `__builtin_prefetch()` for graph/tree traversal

## Common Pitfalls (CRITICAL)

1. **Forgetting to pin pages**: Always pin before access, unpin when done
2. **Writing without WAL**: WAL record must be flushed BEFORE page modification
3. **Page ID 0 confusion**: Page ID 0 is INVALID by convention
4. **CRC before LSN**: Update page LSN BEFORE calculating checksum
5. **Vector dimension mismatch**: All vectors must have identical dimensions
6. **CTest config flags**: Multi-config generators (VS) require `-C Debug`, single-config don't
7. **Test isolation**: Use unique temp directories with timestamp suffixes
8. **Index maintenance**: When modifying Engine Put/Delete, update `key_to_page_` map
9. **DiskManager lifecycle**: Check `IsOpen()` before all write operations

## Testing Conventions

**Framework**: Catch2 v3 with `TEST_CASE` macros (auto-discovered by CMake)

**Test file organization**:
- `tests/test_engine.cpp`: Core operations, WAL recovery, flush/compaction
- `tests/test_advanced_features.cpp`: Batch ops, range scans, metrics
- `tests/test_security.cpp`: Auth, audit logging, RBAC
- `tests/test_web_api.cpp`: HTTP endpoints (marked with `[.]` tag)

**Isolation pattern** (REQUIRED):
```cpp
const auto suffix = static_cast<std::uint64_t>(
    std::chrono::high_resolution_clock::now().time_since_epoch().count());
const auto db_dir = std::filesystem::temp_directory_path() /
                    ("core_engine_test_" + std::to_string(suffix));
```

**Web API test pattern**: Start embedded server in background thread, wait 500ms:
```cpp
std::thread server_thread([&]() {
  Engine engine;
  engine.Open(db_dir);
  httplib::Server server;
  // ... setup endpoints ...
  server.listen("127.0.0.1", port);
});
std::this_thread::sleep_for(std::chrono::milliseconds(500));
httplib::Client client("127.0.0.1", port);
```

## Recent Changes (Jan 2026)

**Performance Optimization (Commit 20ea008)**:
- Added O(1) hash map index (`std::unordered_map<std::string, PageId> key_to_page_`)
- Eliminated O(N) linear page scans on every Get/Put/Delete
- ~1000x speedup for databases with 1000+ pages
- Index rebuilt on `Open()` by scanning existing pages

**Lifecycle Fix (Commit e676c43)**:
- Added defensive checks in BufferPoolManager: `if (!disk_manager_->IsOpen())`
- Prevents "DiskManager not open" errors during shutdown
- Safe destruction order maintained in Engine destructor

**Documentation Cleanup (Commit 6b29fca)**:
- Consolidated 14+ markdown files into `docs/DOCUMENTATION.md`
- Deleted redundant/outdated docs (BUILD_GUIDE, PERFORMANCE, etc.)
- Moved CONTRIBUTING.md and CODE_OF_CONDUCT.md to `docs/`
- Net savings: -6,674 lines (-90% documentation bloat)

## Project Status

**Version**: 1.5 (January 2026)  
**Year 1**: COMPLETE ✅ (Page Layer, BufferPool, LRU-K, WAL, HNSW, Metrics)  
**Next**: Year 2 - io_uring/IOCP async I/O

**Performance Baseline**:
- PUT: 180K ops/sec, 5.2 μs latency
- GET (cached): 850K ops/sec, 1.1 μs latency
- Vector Search: 12K queries/sec, 78 μs latency
