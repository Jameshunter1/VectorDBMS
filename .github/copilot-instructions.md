# LSM Database Engine — AI Coding Agent Instructions

## Project Architecture

This is a **C++20 LSM-tree database engine** with a multi-year design horizon. The architecture follows a layered approach with explicit lifecycle management and minimal dependencies.

### Core Components

- **`Engine`** ([engine.hpp](../src/include/core_engine/engine.hpp)): Stable façade providing Put/Get/Delete API. All subsystems live behind this entry point.
- **`LsmTree`** ([lsm_tree.hpp](../src/include/core_engine/lsm/lsm_tree.hpp)): Storage engine coordinating WAL, MemTable, SSTables, and compaction.
- **`MemTable`** ([memtable.hpp](../src/include/core_engine/lsm/memtable.hpp)): In-memory `std::map` buffering recent writes; flushed at 4 MB threshold.
- **`Wal`** ([wal.hpp](../src/include/core_engine/lsm/wal.hpp)): Write-ahead log providing durability; replayed on startup for crash recovery.
- **`SSTable`** ([sstable.hpp](../src/include/core_engine/lsm/sstable.hpp)): Immutable on-disk sorted key-value files with integrated bloom filters.
- **`Manifest`** ([manifest.hpp](../src/include/core_engine/lsm/manifest.hpp)): Tracks active SSTables for recovery; persisted as `MANIFEST` file.

### Data Flow: Write Path

1. **Durability first**: `LsmTree::Put()` appends to WAL (`wal.log`) before mutating MemTable
2. **In-memory buffering**: Key/value inserted into MemTable (sorted `std::map`)
3. **Automatic flush**: When MemTable reaches 4 MB, flush to new SSTable (`sstable_N.sst`)
4. **Compaction**: When 4+ SSTables exist, merge into one to reduce read amplification
5. **Manifest update**: Each flush/compaction updates `MANIFEST` to track active SSTables

### Data Flow: Read Path

1. **MemTable first**: Check in-memory MemTable for key (hottest data)
2. **SSTable scan**: If not in MemTable, iterate through SSTables (newest to oldest)
3. **Bloom filter optimization**: Each SSTable checks bloom filter before disk read
4. **Tombstone handling**: Deleted keys return `std::nullopt` (marked with `kTombstoneValue`)

## Build System & Workflows

### CMake Structure

- **Multi-root workspace**: `src/` (engine), `tests/` (Catch2), `benchmarks/` (Google Benchmark)
- **CMake Presets**: Use `windows-vs2022-x64-debug` preset for Visual Studio 2022 builds
- **FetchContent dependencies**: Catch2, Google Benchmark, cpp-httplib (fetched during configure)

### Build Commands (from `src/` directory)

```powershell
# Configure (generates build files in src/build/)
& "C:\Program Files\CMake\bin\cmake.exe" --preset windows-vs2022-x64-debug

# Build all targets (library + apps + tests + benchmarks)
& "C:\Program Files\CMake\bin\cmake.exe" --build --preset windows-vs2022-x64-debug

# Run tests via CTest
& "C:\Program Files\CMake\bin\ctest.exe" --test-dir build/windows-vs2022-x64-debug -C Debug --output-on-failure
```

### Developer Workflows

**Quick manual testing**: Use `dbcli` for immediate feedback:
```powershell
.\build\windows-vs2022-x64-debug\Debug\dbcli.exe .\_lsm_demo put key1 value1
.\build\windows-vs2022-x64-debug\Debug\dbcli.exe .\_lsm_demo get key1
```

**Browser UI**: Use `dbweb` for visual interaction:
```powershell
.\build\windows-vs2022-x64-debug\Debug\dbweb.exe .\_web_demo 8080
# Open http://127.0.0.1:8080/
```

## Coding Conventions

### Error Handling

- **Return `Status`, not exceptions**: All storage code returns `core_engine::Status` ([status.hpp](../src/include/core_engine/common/status.hpp))
- **Check before propagate**: Always check `status.ok()` and return early on failure
- **Example pattern**:
  ```cpp
  auto status = wal_.AppendPut(key, value);
  if (!status.ok()) {
    return status;  // Do NOT mutate MemTable if WAL fails
  }
  memtable_.Put(std::move(key), std::move(value));
  ```

### Naming & Style

- **Clang-format settings**: LLVM style, 2-space indent, 100-char line limit ([.clang-format](../src/.clang-format))
- **Namespace**: All engine code in `namespace core_engine`
- **Private members**: Trailing underscore (`memtable_`, `wal_`, `is_open_`)
- **Constants**: `kPascalCase` (e.g., `kMemTableFlushThresholdBytes`, `kTombstoneValue`)

### Tombstone Pattern

Deletes write a special marker (`MemTable::kTombstoneValue = "\x00__TOMBSTONE__\x00"`) instead of removing entries. This shadows older values in SSTables during reads and compaction.

### Thread Safety

- **MemTable**: Coarse-grained mutex (`std::lock_guard`) in Put/Get/Delete
- **LSMTree**: Single-threaded for flush/compaction (starter implementation)
- **Future work**: Lock-free structures, background compaction threads

## Testing Philosophy

- **Catch2 framework**: All tests in [tests/test_engine.cpp](../tests/test_engine.cpp)
- **Temporary directories**: Use `std::filesystem::temp_directory_path()` with unique suffixes
- **Recovery testing**: Verify WAL replay by writing in one Engine instance, reading in another
- **Flush/compaction testing**: Write enough data (e.g., 5000 × 1 KB) to trigger thresholds

## Key Implementation Details

### WAL Recovery Ordering

On `LsmTree::Open()`, **manifest recovery happens BEFORE WAL replay**:
1. Load SSTables from `MANIFEST` (contains flushed data)
2. Replay `wal.log` (contains unflushed data)
3. This ordering prevents duplicate data and maintains consistency

### SSTable File Naming

- Format: `sstable_<id>.sst` where `<id>` is `next_sstable_id_` (auto-incremented)
- Manifest tracks which IDs are active (survives crashes)

### Bloom Filter Integration

- Each SSTable embeds a bloom filter in its header (currently 1000 bits, 3 hash functions)
- **Must check bloom before disk read**: `if (!reader->MayContain(key)) continue;`
- Track statistics: `bloom_checks`, `bloom_hits`, `bloom_false_positives`

## Common Pitfalls

1. **Mutating MemTable before WAL**: Always append to WAL first for crash consistency
2. **Forgetting `-C Debug` in CTest**: Visual Studio is multi-config; must specify build type
3. **Missing manifest updates**: Every flush/compaction must call `manifest_.AddSSTable()` or `manifest_.SetSSTables()`
4. **Bloom false negatives**: Bloom filters can have false positives (say "maybe" when not present) but NEVER false negatives
5. **Tombstone confusion**: Deleted keys return `std::nullopt` but remain in data structures until compaction

## Adding New Features

### Adding a New LSM Component

1. Create header in [include/core_engine/lsm/](../src/include/core_engine/lsm/)
2. Create implementation in [lib/lsm/](../src/lib/lsm/)
3. Add source to `target_sources(core_engine PRIVATE ...)` in [CMakeLists.txt](../src/CMakeLists.txt)
4. Wire into `LsmTree` lifecycle (Open/Close/Flush/Compact)

### Adding Tests

Add `TEST_CASE` in [test_engine.cpp](../tests/test_engine.cpp) using Catch2 macros. Tests auto-discovered via `catch_discover_tests()`.

### Adding Benchmarks

Add new `.cpp` file in [benchmarks/](../benchmarks/), add to `target_sources()` in [benchmarks/CMakeLists.txt](../benchmarks/CMakeLists.txt).

## Subsystem Status

- ✅ **Fully Implemented**: WAL, MemTable, SSTable, single-SSTable flush, bloom filters, manifest, recovery
- ✅ **Structure Complete**: Multi-level LSM (`Level`, `LeveledLSM` classes with L0→L1→L2... hierarchy)
- ⚠️ **Partial**: Leveled compaction (structure exists but automatic compaction disabled pending manifest integration)
- 📋 **Planned**: Manifest updates during leveled compaction, range scans, block cache, compression

### Current Leveled LSM Status

The codebase has a complete implementation of leveled LSM structure:
- [level.hpp](../src/include/core_engine/lsm/level.hpp): `Level` class manages SSTables within a level
- [level.cpp](../src/lib/lsm/level.cpp): `LeveledLSM` class manages multiple levels and compaction logic
- [lsm_tree.hpp](../src/include/core_engine/lsm/lsm_tree.hpp): Integrated `LeveledLSM` into main storage engine

**Why auto-compaction is disabled**: The `LeveledLSM::MaybeCompact()` method works correctly but deletes old SSTable files without updating the `Manifest`. This causes manifest/filesystem desync. The fix requires either:
1. Passing `Manifest&` to `LeveledLSM` for direct updates, OR  
2. Having `LeveledLSM` return a list of added/removed IDs for `LsmTree` to update

See comment in [lsm_tree.cpp FlushMemTable()](../src/lib/lsm/lsm_tree.cpp) for details.
