# Vectis - Source Code

This directory contains the core C++20 database engine implementation.

## Directory Structure

\\\
src/
 include/core_engine/    # Public API headers
    common/             # Status, config, types
    storage/            # Page, disk, buffer pool, WAL
    vector/             # HNSW index, distance metrics
    security/           # Auth, audit logging
    config/             # Application configuration
 lib/                    # Implementation (.cpp files)
    storage/
    vector/
    security/
 apps/                   # Executable applications
    dbcli/              # Interactive CLI
    dbweb/              # HTTP server
 CMakeLists.txt          # Build configuration
\\\

## Building

\\\ash
# From repository root
cmake --preset windows-vs2022-x64-debug -S src
cmake --build build/windows-vs2022-x64-debug --config Debug
\\\

## Running Tests

\\\ash
ctest --test-dir build/windows-vs2022-x64-debug -C Debug --output-on-failure
\\\

## Key Components

- **Page** (4 KB): Fixed-size storage unit with LSN, checksum, pin count
- **DiskManager**: Raw block I/O with O_DIRECT support
- **BufferPoolManager**: LRU-K page cache with eviction
- **LogManager**: ARIES-style WAL for crash recovery
- **HNSWIndex**: Vector similarity search with O(log N) complexity
- **Engine**: High-level database API

See [../docs/DOCUMENTATION.md](../docs/DOCUMENTATION.md) for complete documentation.
