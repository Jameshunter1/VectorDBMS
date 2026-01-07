# LSM Database Engine - AI Agent Instructions

## Project Overview

This is a **production-ready LSM (Log-Structured Merge-tree) database engine** in C++20. It's a high-performance key-value store with ACID guarantees, security features, and web management interface.

**Current Version**: 1.4 (Performance & Advanced Features)  
**Architecture**: Modular LSM-tree with separate subsystems for storage, transactions, security, and web API.

## Critical Architecture Concepts

### The LSM Write/Read Flow (Core to Everything)

**Write Path** (`Put(key, value)`):
1. **WAL first** ([lib/lsm/wal.cpp](lib/lsm/wal.cpp)) - durability guarantee, append-only log
2. **MemTable** ([lib/lsm/memtable.cpp](lib/lsm/memtable.cpp)) - in-memory sorted map (std::map)
3. **Flush trigger** - when MemTable hits 4MB threshold → SSTable creation
4. **Compaction** ([lib/lsm/level.cpp](lib/lsm/level.cpp)) - background merging of SSTables across levels (L0→L1→L2...)

**Read Path** (`Get(key)`):
1. Check MemTable (fastest)
2. Check SSTables in reverse order (newest first) using **Bloom filters** to skip files
3. Return first match or NOT_FOUND

**Tombstones**: Deletes write special marker `\x00__TOMBSTONE__\x00` - propagates through compaction until oldest level.

### Multi-Root Workspace Structure

```
CORE-ENGINE/ (./src)           - Engine library + apps
TEST-SUITE/ (./tests)           - Catch2 test executables  
BENCHMARKS/ (./benchmarks)      - Performance benchmarks
```

**Root CMakeLists.txt**: [CMakeLists.txt](CMakeLists.txt) at `src/` adds sibling folders via `add_subdirectory(../tests ...)`.

### Key Subsystems

- **`core_engine::Engine`** ([engine.hpp](include/core_engine/engine.hpp)) - Main API facade
- **`LsmTree`** ([lsm/lsm_tree.cpp](lib/lsm/lsm_tree.cpp)) - Coordinates WAL/MemTable/SSTables/Compaction
- **`DatabaseConfig`** ([common/config.hpp](include/core_engine/common/config.hpp)) - Separate paths for data/WAL (production mode)
- **`AuthManager`** ([security/auth.cpp](lib/security/auth.cpp)) - bcrypt passwords, session management
- **`AuditLog`** ([security/audit.cpp](lib/security/audit.cpp)) - 14 event types logged to audit.log
- **`Manifest`** ([lsm/manifest.cpp](lib/lsm/manifest.cpp)) - Tracks SSTable metadata for recovery

## Build System (CMake 3.24+)

### Standard Commands

```powershell
# Windows (MSVC with Ninja)
cmake --preset windows-msvc-debug
cmake --build --preset debug
ctest --preset debug

# Windows (Visual Studio generator)
cmake --preset windows-vs2022-x64-debug
cmake --build build/windows-vs2022-x64-debug --config Debug

# Linux/Mac
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build
```

### Build Options

Defined in [CMakeLists.txt:28-33](CMakeLists.txt#L28-L33):
- `CORE_ENGINE_BUILD_TESTS=ON` - Build test suite (Catch2)
- `CORE_ENGINE_BUILD_BENCHMARKS=ON` - Build benchmarks
- `CORE_ENGINE_BUILD_WEB=ON` - Build dbweb server (httplib)
- `CORE_ENGINE_WARNINGS_AS_ERRORS=OFF` - Treat warnings as errors

### CMake Helper Functions

See [cmake/ProjectOptions.cmake](cmake/ProjectOptions.cmake):
- `core_engine_apply_project_options(target)` - Standard compile flags
- `core_engine_apply_warnings(target, as_errors)` - /W4 or -Wall -Wextra
- `core_engine_apply_sanitizers(target, enable)` - ASan/UBSan (Clang/GCC only)

Apply to all new targets/executables.

## Development Workflows

### Running Tests

```powershell
# All tests
ctest --preset debug --output-on-failure

# Specific test binary
.\build\windows-vs2022-x64-debug\Debug\core_engine_tests.exe
.\build\windows-vs2022-x64-debug\Debug\security_tests.exe
```

Test structure: [tests/](../tests) uses Catch2. Test file naming: `test_*.cpp`.

### Running Web Interface

```powershell
# Quick demo (auto-opens browser)
.\demo_web_simple.ps1

# Manual (embedded mode)
.\build\windows-vs2022-x64-debug\Debug\dbweb.exe .\_web_demo 8080

# Production mode (separate data/WAL dirs)
.\build\windows-vs2022-x64-debug\Release\dbweb.exe ./data --wal-dir ./wal --port 8080
```

Web interface: [apps/dbweb/main.cpp](apps/dbweb/main.cpp) - embedded HTML, REST API with httplib.

### Demo Scripts

- `demo_web_simple.ps1` - Web UI on port 8080
- `demo_compaction.ps1` - Visualize LSM compaction process  
- `demo_web_enhanced.ps1` - Enhanced UI with pagination/search

## Code Conventions

### Namespace Structure

All code in `namespace core_engine`. Subsystems use nested namespaces:
- `core_engine::security` - Auth/audit
- `core_engine::storage` - Page files (future)
- No global `using namespace` in headers.

### Status Handling

Use `Status` ([common/status.hpp](include/core_engine/common/status.hpp)) for error propagation:
```cpp
Status s = lsm_tree_.Put(key, value);
if (!s.ok()) {
  return s;  // Bubble up error
}
```

**Never throw exceptions** in core engine code - Status returns only.

### Header Organization

- **Public API**: `include/core_engine/` - installed headers
- **Implementation**: `lib/` - .cpp files mirror header structure
- **Apps**: `apps/dbcli`, `apps/dbweb` - executables linking core_engine library

### Threading Model

Current (v1.4): **Coarse-grained locking**
- MemTable: single `std::mutex` ([memtable.hpp:67](include/core_engine/lsm/memtable.hpp#L67))
- LsmTree: reader-writer lock for MemTable/SSTable list
- Future: Lock-free data structures, thread-per-level compaction

### File Naming Patterns

- SSTables: `{level}_{sequence}.sst` (e.g., `0_00001.sst` in `level_0/`)
- WAL: `wal.log` in configured WAL directory
- Manifest: `MANIFEST` in data directory (tracks SSTable metadata)
- Audit log: `audit.log` in root directory

## Configuration Patterns

### Embedded vs Production Mode

```cpp
// Embedded (SQLite-style, single directory)
Engine engine;
engine.Open("./my_database");  // data, WAL, SSTables all in ./my_database/

// Production (separate fast/capacity disks)
DatabaseConfig config = DatabaseConfig::Production(
  "/var/lib/lsmdb/data",   // SSTables on capacity disk
  "/var/lib/lsmdb/wal"     // WAL on fast NVMe
);
engine.Open(config);
```

See [common/config.hpp:60-90](include/core_engine/common/config.hpp#L60-L90) for factory methods.

### Security Configuration

Authentication required in production. Example:
```cpp
AuthManager auth;
auth.CreateUser("admin", "password123", {"admin"});
std::string token = auth.CreateSession("admin", "127.0.0.1");
bool valid = auth.ValidateSession(token);
```

Audit events logged automatically: AUTH_LOGIN, PUT, DELETE, etc. ([security/audit.cpp:19-35](lib/security/audit.cpp#L19-L35))

## Performance Characteristics

**v1.4 Benchmarks** (see [MILESTONE_V1.4_ADVANCED.md](MILESTONE_V1.4_ADVANCED.md)):
- Single Put: ~50,000 ops/sec (~20 µs/op)
- Batch Put (100 ops): ~67,000 ops/sec (~15 µs/op) - **8x faster** than individual
- MemTable Get: ~500,000 ops/sec (< 10 µs)
- SSTable Get (Bloom hit): ~100,000 ops/sec (< 500 µs)
- Range Scan: ~40,000 keys/sec

**Optimization Priorities**:
1. Use `BatchWrite()` for bulk operations ([engine.hpp:64-68](include/core_engine/engine.hpp#L64-L68))
2. Enable Bloom filters (auto-enabled in SSTables)
3. Configure MemTable flush threshold based on workload ([config.hpp:41](include/core_engine/common/config.hpp#L41))

## Common Pitfalls

1. **Forgetting WAL-first ordering**: Always write to WAL before MemTable ([lsm_tree.cpp:68-75](lib/lsm/lsm_tree.cpp#L68-L75))
2. **Missing manifest updates**: Flush/compaction must call `manifest_.AddSSTable()` ([lsm_tree.cpp:156](lib/lsm/lsm_tree.cpp#L156))
3. **Tombstone propagation**: Deletes must reach oldest level or data reappears ([memtable.hpp:58](include/core_engine/lsm/memtable.hpp#L58))
4. **Config paths**: Production mode requires separate data/WAL dirs - use `DatabaseConfig::Production()` ([config.hpp:60](include/core_engine/common/config.hpp#L60))

## Testing Strategy

- **Unit tests**: [tests/test_engine.cpp](../tests/test_engine.cpp) - Put/Get/Delete, WAL recovery
- **Security tests**: [tests/test_security.cpp](../tests/test_security.cpp) - Auth, sessions, audit
- **Integration tests**: [tests/test_integration_security.cpp](../tests/test_integration_security.cpp) - Multi-user scenarios
- **Web API tests**: [tests/test_web_api.cpp](../tests/test_web_api.cpp) - REST endpoint validation

**Test isolation**: Each test uses unique temp directory (see [test_engine.cpp:13-17](../tests/test_engine.cpp#L13-L17)).

## Documentation Structure

- **README.md** - Quick start, features, build instructions
- **EXECUTIVE_SUMMARY.md** - Project stats, deployment overview
- **MILESTONE_V1.4_ADVANCED.md** - Latest features (batch ops, range scans, rate limiting)
- **DEPLOYMENT_GUIDE.md** - Production deployment (cloud, systemd, Nginx)
- **QUICK_REFERENCE.md** - API reference, configuration options

## Adding New Features

### New Storage Feature (e.g., compression)
1. Add to SSTable writer ([lsm/sstable.cpp](lib/lsm/sstable.cpp))
2. Update manifest format ([lsm/manifest.cpp](lib/lsm/manifest.cpp))
3. Add config option ([common/config.hpp](include/core_engine/common/config.hpp))
4. Write unit tests ([tests/](../tests))

### New API Endpoint (Web)
1. Add REST handler in [apps/dbweb/main.cpp](apps/dbweb/main.cpp)
2. Update HTML/JavaScript in embedded `kIndexHtml`
3. Add authentication check if needed
4. Test with [tests/test_web_api.cpp](../tests/test_web_api.cpp)

### New Security Feature
1. Implement in [lib/security/](lib/security/)
2. Add audit event type ([security/audit.hpp](include/core_engine/security/audit.hpp))
3. Update [tests/test_security.cpp](../tests/test_security.cpp)
4. Document in **MILESTONE_V1.3_SECURITY.md**
