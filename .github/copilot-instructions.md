# Vectis — AI Coding Agent Instructions

## Project Architecture

This is a **C++20 page-oriented vector database engine** designed for production use in AI/ML applications. The architecture follows a hardware-aware, explicit-control philosophy optimized for NVMe, SIMD, and modern CPU architectures.

**Design Philosophy**: Treat the OS as a slow coordinator. Explicit memory management, deterministic behavior, and performance transparency over convenience.

**Repository Structure**:
- `src/`: Main codebase root - all CMake configuration starts here
- `src/include/core_engine/`: Public headers organized by subsystem
- `src/lib/`: Implementation files mirroring include structure
- `src/apps/`: Executables (dbcli, dbweb, tutorial examples)
- `tests/`: Catch2 test suite (separate CMakeLists.txt)
- `benchmarks/`: Google Benchmark performance tests
- `build/`: Generated build artifacts (gitignored, created by CMake)

### Core Components (Storage Layer - Implemented)

- **`Page`** ([page.hpp](../src/include/core_engine/storage/page.hpp)): 4 KB fixed-size page with LSN, pin_count, dirty flag, and CRC32 checksum. Cache-line aligned for optimal CPU performance. ✅ COMPLETE
- **`DiskManager`** ([disk_manager.hpp](../src/include/core_engine/storage/disk_manager.hpp)): Raw block I/O for page allocation and atomic writes. File-based implementation (O_DIRECT planned for Year 2). ✅ COMPLETE
- **`BufferPoolManager`** ([buffer_pool_manager.hpp](../src/include/core_engine/storage/buffer_pool_manager.hpp)): Thread-safe page cache with frame pinning, dirty page tracking, and LRU-K eviction. ✅ COMPLETE
- **`LogManager`** ([log_manager.hpp](../src/include/core_engine/storage/log_manager.hpp)): Sequential WAL with ARIES-style recovery (Analysis, Redo, Undo). ✅ COMPLETE
- **`PageFile`** ([page_file.hpp](../src/include/core_engine/storage/page_file.hpp)): Higher-level abstraction over BufferPoolManager for KV storage with MemTable/SSTable architecture. ✅ COMPLETE

### Vector Database Components (Implemented)

- **`HNSWIndex`** ([hnsw_index.hpp](../src/include/core_engine/vector/hnsw_index.hpp)): Hierarchical Navigable Small World graph for O(log N) approximate nearest neighbor search. Thread-safe with shared_mutex. ✅ COMPLETE
- **`Vector`** ([vector.hpp](../src/include/core_engine/vector/vector.hpp)): SIMD-optimized distance calculations (Cosine, Euclidean, Dot Product, Manhattan). Float32 aligned for AVX2/AVX-512. ✅ COMPLETE
- **`DatabaseConfig::vector_*`** ([config.hpp](../src/include/core_engine/common/config.hpp)): Configuration for vector features: dimension, metric, HNSW params (M, ef_construction, ef_search). ✅ COMPLETE

### Security & Monitoring Components (Implemented)

- **`AuthManager`** ([security/auth.hpp](../src/include/core_engine/security/auth.hpp)): User authentication with session management and role-based access control (RBAC).
- **`AuditLogger`** ([security/audit.hpp](../src/include/core_engine/security/audit.hpp)): Comprehensive audit logging for security events (login, access, operations).
- **`RateLimiter`** ([rate_limiter.hpp](../src/include/core_engine/rate_limiter.hpp)): Token bucket rate limiting with per-client and per-endpoint configuration.
- **`MetricsCollector`** ([metrics.hpp](../src/include/core_engine/metrics.hpp)): Prometheus-compatible metrics export (counters, gauges, histograms).
- **`AppConfig`** ([config/app_config.hpp](../src/include/core_engine/config/app_config.hpp)): Application-level configuration with presets (Development, Production).

### Data Flow: Write Path (Page-Based)

1. **WAL first**: `Engine::Put()` → `LogManager::AppendLog()` → Sequential append to WAL (group commit for batching)
2. **Buffer pool fetch**: Request target page via `BufferPoolManager::FetchPage(page_id)` → Pin page in memory
3. **In-page modification**: Update page content (B-tree node, heap tuple, index entry)
4. **Mark dirty**: Set page dirty flag for later flush
5. **Unpin**: Release pin, page now eligible for eviction via LRU-K
6. **Async flush**: Background thread or explicit checkpoint writes dirty pages to disk
7. **Vector indexing**: Vectors stored as serialized page records, HNSW graph nodes reference page_ids

### Data Flow: Read Path (Page-Based)

1. **Buffer pool check**: `BufferPoolManager::FetchPage(page_id)` checks page table
2. **Cache hit**: If in memory, increment pin_count and return frame pointer
3. **Cache miss**: Evict victim page via LRU-K, load from disk via `DiskManager::ReadPage()`
4. **Page validation**: Verify CRC32 checksum, check LSN consistency
5. **Return pinned page**: Caller accesses page data, unpins when done
6. **Vector search**: HNSW traversal fetches graph nodes from buffer pool, SIMD distance calculations on aligned vector data

### Data Flow: Recovery Path (ARIES-style)

1. **Analysis pass**: Scan WAL forward from checkpoint to identify uncommitted transactions
2. **Redo pass**: Replay all logged operations to reconstruct buffer pool state
3. **Undo pass**: Roll back uncommitted transactions
4. **Checkpoint**: Flush all dirty pages and write checkpoint LSN
5. **Vector index reconstruction**: HNSW graph persisted as pages, recovered during normal page loading

## Build System & Workflows

### CMake Structure

- **Multi-root workspace**: `src/` (engine), `tests/` (Catch2), `benchmarks/` (Google Benchmark)
- **Build directory**: `build/` at repository root (out-of-source builds)
- **Source directory**: `src/` contains CMakeLists.txt (project root for CMake)
- **Compiler flags**: `-march=native` for SIMD, `-O3 -DNDEBUG` for release, `/std:c++20` on Windows

**Directory Layout:**
```
VectorDBMS/                  # Repository root (run CMake commands from here)
├── src/                     # Source directory (CMakeLists.txt here)
│   ├── CMakeLists.txt       # Main project configuration
│   ├── CMakePresets.json    # Preset configurations
│   ├── include/             # Public headers
│   ├── lib/                 # Implementation files
│   └── apps/                # Executables (dbcli, dbweb, tutorial)
├── build/                   # Build directory (created by CMake)
│   └── windows-vs2022-x64-debug/  # Build artifacts for specific preset
├── tests/                   # Test suite (sibling to src/)
└── benchmarks/              # Performance benchmarks (sibling to src/)
```

### Build Commands (from repository root)

```powershell
# Windows: Configure with Visual Studio 2022 generator (multi-config)
# IMPORTANT: Run from repository root, NOT from src/ directory
cmake --preset windows-vs2022-x64-debug -S src

# Build all targets (library + apps + tests + benchmarks)
cmake --build --preset windows-vs2022-x64-debug

# Run tests via CTest (MUST specify -C Debug for multi-config generators like VS)
ctest --test-dir build/windows-vs2022-x64-debug -C Debug --output-on-failure

# Or use test preset (includes -C Debug automatically)
ctest --preset windows-vs2022-x64-debug --output-on-failure

# Run specific test
.\build\windows-vs2022-x64-debug\Debug\core_engine_tests.exe "[engine]"

# Run benchmarks
.\build\windows-vs2022-x64-debug\benchmarks\Debug\core_engine_benchmarks.exe --benchmark_filter=Page
```

```bash
# Linux/macOS: Configure with Ninja or Unix Makefiles (single-config)
# Run from repository root
cmake -B build -S src -DCMAKE_BUILD_TYPE=Debug -DCORE_ENGINE_BUILD_TESTS=ON

# Build
cmake --build build -j

# Run tests (no -C flag needed for single-config generators)
ctest --test-dir build --output-on-failure

# Run specific test
./build/tests/core_engine_tests "[engine]"

# Run benchmarks
./build/benchmarks/core_engine_benchmarks --benchmark_filter=Page
```

**Key differences**:
- **Multi-config generators** (Visual Studio, Xcode): Build directory contains multiple configurations (Debug/Release), requires `-C Debug` in ctest
- **Single-config generators** (Ninja, Unix Makefiles): Build directory is specific to one config, no `-C` flag needed
- Test executables are in `Debug/` subdirectory on Windows with VS generator, root build dir on Linux

### Developer Workflows

**Testing pattern**: All tests use temporary directories with unique suffixes to avoid conflicts:
```cpp
const auto suffix = static_cast<std::uint64_t>(
    std::chrono::high_resolution_clock::now().time_since_epoch().count());
const auto db_dir = std::filesystem::temp_directory_path() /
                    ("core_engine_test_db_" + std::to_string(suffix));
```

**Vector operations**: Configure via `DatabaseConfig`:
```cpp
DatabaseConfig config = DatabaseConfig::Embedded("./vector_db");
config.enable_vector_index = true;
config.vector_dimension = 128;  // Must match your embeddings
config.vector_metric = DatabaseConfig::VectorDistanceMetric::kCosine;
config.hnsw_params.M = 16;                // Graph connectivity
config.hnsw_params.ef_construction = 200; // Build quality
config.hnsw_params.ef_search = 50;        // Query quality
config.buffer_pool_size = 1024;           // 4 MB buffer pool (1024 pages × 4 KB)

Engine engine;
engine.Open(config);
engine.PutVector("doc1", vector::Vector({0.1f, 0.2f, /* ... 128 dims */}));
auto results = engine.SearchSimilar(query_vec, /*k=*/10);
```

**Web API testing**: Tests start embedded HTTP servers in background threads:
```cpp
// Server runs in separate thread with Engine instance
std::thread server_thread([&]() {
  Engine engine;
  engine.Open(db_dir);
  httplib::Server server;
  server.Post("/api/put", [&](const httplib::Request& req, httplib::Response& res) {
    /* ... */
  });
  server.listen("127.0.0.1", port);
});
// Wait for server startup, then use httplib::Client for tests
std::this_thread::sleep_for(std::chrono::milliseconds(500));
httplib::Client client("127.0.0.1", port);
auto res = client.Get("/api/get?key=test");
```

### Testing Conventions

- **Test framework**: Catch2 v3 with `TEST_CASE` macros
- **Test discovery**: CMake's `catch_discover_tests()` automatically finds all test cases
- **Test files**: 
  - [tests/test_engine.cpp](../tests/test_engine.cpp): Core engine operations, WAL recovery, flush/compaction
  - [tests/test_advanced_features.cpp](../tests/test_advanced_features.cpp): Batch operations, range scans, rate limiting, metrics
  - [tests/test_security.cpp](../tests/test_security.cpp): Auth, audit logging, RBAC, sessions
  - [tests/test_web_api.cpp](../tests/test_web_api.cpp): HTTP API endpoints, error handling, integration
- **Temporary directories**: Use `std::filesystem::temp_directory_path()` with unique suffixes for isolation
- **Recovery testing**: Verify WAL replay by writing in one Engine instance, closing it, then reading in another
- **Flush/compaction testing**: Write enough data (e.g., 5000 × 1 KB values) to trigger 4 MB threshold
- **Web API tests**: Marked with `[.]` tag, start embedded HTTP servers in background threads with 500ms startup delay

## Coding Conventions

 - Use C++20 features (concepts, ranges, smart pointers) where appropriate for clarity and safety
 - Follow LLVM coding style (see [.clang-format](../src/.clang-format))

### Error Handling

- **Return `Status`, not exceptions**: All storage code returns `core_engine::Status` ([status.hpp](../src/include/core_engine/common/status.hpp))
- **Check before propagate**: Always check `status.ok()` and return early on failure
- **Example pattern**:
  ```cpp
  auto status = log_manager_.AppendLog(log_record);
  if (!status.ok()) {
    return status;  // Do NOT modify page if WAL fails
  }
  page->MarkDirty();
  ```

### Memory Management Philosophy

- **Explicit alignment**: Use `posix_memalign` or `aligned_alloc` for 64-byte cache-line alignment
- **No hidden allocations**: Every allocation point must be visible and justified
- **RAII for pins**: Page pins use guard objects (`PageGuard`) to prevent leaks
- **Zero-copy where possible**: Pass frame pointers directly, avoid buffer copies

### Naming & Style

- **Clang-format settings**: LLVM style, 2-space indent, 100-char line limit ([.clang-format](../src/.clang-format))
- **Namespace**: All engine code in `namespace core_engine`
- **Private members**: Trailing underscore (`buffer_pool_`, `disk_manager_`, `is_open_`)
- **Constants**: `kPascalCase` (e.g., `kPageSize`, `kInvalidPageId`, `kMaxFrames`)
- **Every line commented**: All new code should have line-by-line comments explaining **why**, not just **what** (cache behavior, TLBs, syscalls)

### Thread Safety

- **BufferPoolManager**: Fine-grained latches per page frame, global latch for page table
- **Page latches**: Read/write latches for concurrent page access
- **HNSWIndex**: shared_mutex for concurrent reads, exclusive write lock for inserts
- **Lock-free data structures**: Preferred where contention is high (free list, log buffer)

### Performance-Critical Patterns

- **Avoid virtual dispatch in hot paths**: Use templates or function pointers
- **Prefetch during traversal**: `__builtin_prefetch()` for graph/tree navigation
- **Batch operations**: Group commit for WAL, batch page flushes for I/O efficiency
- **SIMD intrinsics**: Use AVX2/AVX-512 for vector distance calculations (see [vector.hpp](../src/include/core_engine/vector/vector.hpp))
- **Cache-line alignment**: Page headers are 64 bytes to fit in single cache line
- **Memory pooling**: Buffer pool pre-allocates frames to avoid dynamic allocation in hot path/test_engine.cpp) and related test files
- **Temporary directories**: Use `std::filesystem::temp_directory_path()` with unique suffixes
- **Recovery testing**: Verify WAL replay by writing in one Engine instance, reading in another
- **Flush/compaction testing**: Write enough data (e.g., 5000 × 1 KB) to trigger thresholds
- **Vector testing**: Tests for HNSW index quality, recall, and performance in [tests/test_advanced_features.cpp](../tests/test_advanced_features.cpp)
- **Security testing**: Auth, audit, and rate limiting tests in [tests/test_security.cpp](../tests/test_security.cpp)
- **Integration tests**: Web API and end-to-end scenarios in [tests/test_web_api.cpp](../tests/test_web_api.cpp)

## Key Implementation Details

### Configuration System

The database has three deployment modes (see [config.hpp](../src/include/core_engine/common/config.hpp)):
- **`DatabaseConfig::Embedded(path)`**: Single directory, SQLite-style (dev/desktop apps)
- **`DatabaseConfig::Production(path)`**: Separate data/WAL dirs, systemd-ready (servers)
- **`DatabaseConfig::Development(path)`**: Project-relative paths (testing)

All configurations support:
- Level-specific directories (`data_dir/level_0/`, `data_dir/level_1/`, etc.)
- Separate WAL directory for performance (fast SSD for WAL, capacity HDD for data)
- Configurable flush/compaction thresholds
- Vector index configuration (dimension, metric, HNSW params)

### WAL Recovery Ordering

On database recovery, **page loading happens BEFORE WAL replay**:
1. Load existing pages from disk
2. Replay `wal.log` (contains unflushed data)
3. This ordering prevents duplicate data and maintains consistency

### Page File Naming

- Format: `pages.db` - single database file containing all pages
- Page addressing: offset = page_id × 4096 bytes
- Manifest tracks metadata (future: page allocation bitmap)

### Bloom Filter Integration

Page-based storage doesn't use bloom filters yet. Future enhancements:

### Vector Index Integration

- Vectors stored as page records (key → vector bytes)
- HNSW index maintained separately in memory for fast similarity search
- On recovery, vectors rebuilt from page storage (future: persist HNSW graph to disk)
- Batch operations for efficient bulk loading: `BatchPutVectors()`, `BatchGetVectors()`

## Common Pitfalls

1. **Forgetting to pin pages**: Always pin before accessing, unpin when done (use `PageGuard` RAII)
2. **Writing dirty pages without WAL**: WAL record must be flushed **before** page flush (LSN ordering)
3. **Unaligned I/O**: All O_DIRECT I/O must be aligned to sector size (typically 512 bytes)
4. **Page id 0 confusion**: Page id 0 is **invalid** by convention (use `kInvalidPageId`)
5. **CRC before LSN**: Update page LSN **before** calculating checksum
6. **Vector dimension mismatch**: All vectors in an index must have identical dimensions (validated at insert time)
7. **SIMD alignment**: Vectors must be 32-byte aligned for AVX2, 64-byte for AVX-512
8. **CTest configuration flags**: Multi-config generators (VS) require `-C Debug`, single-config (Ninja) don't
9. **Test isolation**: Use unique temp directories with timestamp suffixes to avoid conflicts between parallel tests

## Adding New Features

### Adding a New Page Type

1. Define page type enum in [page_types.hpp](../src/include/core_engine/storage/page_types.hpp)
2. Create specialized page class (e.g., `BTreePage`, `HeapPage`)
3. Implement serialization/deserialization
4. Add type-specific operations (GetRecord, InsertRecord, Split, etc.)
5. Wire into DiskManager page allocation

### Adding Tests

Add `TEST_CASE` in appropriate test file ([test_storage.cpp](../tests/test_storage.cpp), [test_vector.cpp](../tests/test_vector.cpp)) using Catch2 macros. Tests auto-discovered via `catch_discover_tests()`.

**Test file guidelines**:
- Use unique temporary directories: `std::filesystem::temp_directory_path() / ("test_name_" + std::to_string(timestamp))`
- Web API tests need `[.]` tag and 500ms startup delay for embedded server
- Recovery tests: write in one `Engine` instance, close it, verify in new instance
- Flush tests: write 5000+ KBfor 4 MB threshold

### Adding SIMD-Optimized Functions

1. Create architecture-specific implementations (#ifdef for AVX2, AVX-512, NEON)
2. Use runtime CPU feature detection (`__builtin_cpu_supports()`)
3. Fall back to scalar implementation if SIMD unavailable
4. Benchmark on target hardware (micro-benchmarks + real workloads)

## Year 1 Roadmap Status

### Q1: Disk & Page Layer ✅ **COMPLETE**
- ✅ Page structure with LSN, pin_count, dirty, checksum
- ✅ DiskManager with O_DIRECT support
- ✅ Aligned I/O, CRC32 validation
- ✅ Unit tests + benchmarks

### Q2: Buffer Pool Manager ✅ **COMPLETE**
- ✅ Thread-safe BufferPoolManager
- ✅ PageTable (page_id → frame index)
- ✅ FreeList management
- ✅ Pin/Unpin/Flush operations

### Q3: LRU-K Eviction ✅ **COMPLETE**
- ✅ LRU-K eviction integrated into BufferPoolManager
- ✅ Evict pages with maximum backward K-distance
- ✅ Integration with BufferPoolManager

### Q4: Write-Ahead Logging ✅ **COMPLETE**
- ✅ LogManager with sequential append
- ✅ Group commit optimization
- ✅ ARIES-style recovery (Analysis, Redo, Undo)

## Performance Characteristics (Year 1 Baseline)

- **Page read latency**: ~5-10 μs (buffer pool hit), ~100-200 μs (NVMe miss with O_DIRECT)
- **Page write throughput**: ~500K IOPS on modern NVMe (queue depth 32, O_DIRECT)
- **Buffer pool hit rate**: >95% for typical workloads (LRU-K with K=2)
- **WAL group commit**: 10-100x throughput improvement vs per-write fsync
- **Vector search**: O(log N) with HNSW, ~1-5ms for 10K vectors, <10ms for 1M vectors
- **SIMD acceleration**: 5-10x speedup for distance calculations (AVX2 vs scalar)
- **Memory overhead**: 8 bytes per page (page table entry) + frame metadata

## Five-Year Strategic Roadmap

**Year 1 — Storage Engine Foundations** ✅ **COMPLETE**
- Q1: ✅ Disk & Page Layer
- Q2: ✅ Buffer Pool Manager  
- Q3: ✅ LRU-K Eviction
- Q4: ✅ Write-Ahead Logging

**Year 2 — Advanced Memory & Async I/O**
- io_uring integration (Linux)
- IOCP (Windows)
- Zero-copy buffer registration
- User-space paging experiments

**Year 3 — Networking & Protocols**
- io_uring-based TCP server
- Custom binary protocol
- Connection pooling
- P99 latency optimization

**Year 4 — Vector Indexing** (HNSW Already Done!)
- ✅ HNSW implementation complete
- Product Quantization for compression
- Hybrid search (vector + metadata filters)
- GPU acceleration exploration

**Year 5 — Scalability & Commercialization**
- Sharding and replication
- Multi-tenancy
- Client SDKs (Python, JS, Go)
- Managed SaaS offering
