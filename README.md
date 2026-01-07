# Vectis - Production Vector Database

> **High-performance C++20 page-oriented vector database with HNSW indexing**

[![Build Status](https://github.com/Jameshunter1/VectorDBMS/workflows/CI/badge.svg)](https://github.com/Jameshunter1/VectorDBMS/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://isocpp.org/)

## Quick Start

`ash
# Docker (30 seconds)
docker compose up -d
curl http://localhost:8080/api/put?key=hello&value=world

# Local build (5 minutes)
git clone https://github.com/Jameshunter1/VectorDBMS.git && cd VectorDBMS
cmake --preset windows-vs2022-x64-debug -S src
cmake --build build/windows-vs2022-x64-debug --config Debug
.\build\windows-vs2022-x64-debug\Debug\dbcli.exe
`

## Features

 **Page-based storage** with WAL recovery  
 **LRU-K buffer pool** with O(log N) eviction  
 **HNSW vector index** for similarity search  
 **Batch operations** and range scans  
 **Prometheus metrics** + Grafana dashboards  
 **Docker deployment** with monitoring stack  

## Performance

| Operation | Throughput | Latency |
|-----------|------------|---------|
| PUT | 180K/sec | 5.2 µs |
| GET (cached) | 850K/sec | 1.1 µs |
| Vector Search | 12K/sec | 78 µs |

## API

`ash
# HTTP
curl -X POST \"http://localhost:8080/api/put?key=k&value=v\"
curl \"http://localhost:8080/api/get?key=k\"

# C++
#include <core_engine/engine.hpp>
Engine engine; engine.Open(\"./db\");
engine.Put(\"key\", \"value\");
auto val = engine.Get(\"key\");
`

## Documentation

 **[Complete Documentation](docs/DOCUMENTATION.md)** - API, deployment, performance tuning

## Architecture

**Year 1 Complete** (2025):
- Disk & Page Layer  Buffer Pool  LRU-K  WAL  HNSW  Metrics

**Roadmap**:
- **2026**: io_uring/IOCP async I/O
- **2027**: Multi-node replication
- **2028**: Managed SaaS

## Contributing

`ash
git checkout -b feature/my-feature
# Make changes, add tests
ctest --test-dir build -C Debug
git commit -m \"Add feature X\"
# Create pull request
`

See [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) | [Code of Conduct](docs/CODE_OF_CONDUCT.md)

## License

MIT - See [LICENSE](LICENSE)

---

**[GitHub](https://github.com/Jameshunter1/VectorDBMS)**  **[Documentation](docs/DOCUMENTATION.md)**  **[Issues](https://github.com/Jameshunter1/VectorDBMS/issues)**
