# Vectis - Source Code

This directory contains the core C++20 database engine implementation.

## Directory Structure

src/
   include/core_engine/    # Public API headers
      common/             # Status, config, types
      storage/            # Page, disk, buffer pool, WAL
      vector/             # HNSW index, distance metrics
      security/           # Auth, audit logging
      config/             # Application configuration
   lib/                  # Implementation (.cpp files)
   apps/                 # Executable applications
      dbcli/              # Interactive CLI
      dbweb/              # HTTP server
   CMakeLists.txt        # Build configuration

---
For all build, usage, and API details, see the main [DOCUMENTATION.md](../docs/DOCUMENTATION.md).
