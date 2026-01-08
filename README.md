# Vectis - Production Vector Database

> **High-performance C++20 page-oriented vector database with HNSW indexing**

[![Build Status](https://github.com/Jameshunter1/VectorDBMS/workflows/CI/badge.svg)](https://github.com/Jameshunter1/VectorDBMS/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://isocpp.org/)

## Quick Start

```bash
# Docker (30 seconds)
docker compose up -d
curl http://localhost:8080/api/put?key=hello&value=world

# Local build - Windows
git clone https://github.com/Jameshunter1/VectorDBMS.git && cd VectorDBMS
cmake --preset windows-vs2022-x64-debug -S src
cmake --build build/windows-vs2022-x64-debug --config Debug
.\build\windows-vs2022-x64-debug\Debug\dbcli.exe

# Local build - Linux/macOS
git clone https://github.com/Jameshunter1/VectorDBMS.git && cd VectorDBMS
cmake -B build -S src -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/dbcli
```

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
| PUT | 180K/sec | 5.2 �s |
| GET (cached) | 850K/sec | 1.1 �s |
| Vector Search | 12K/sec | 78 �s |

## API

```bash
# HTTP REST API
curl -X POST "http://localhost:8080/api/put?key=mykey&value=myvalue"
curl "http://localhost:8080/api/get?key=mykey"
curl "http://localhost:8080/api/stats"
```

```cpp
// C++ Library
#include <core_engine/engine.hpp>

core_engine::Engine engine;
engine.Open("./mydb");
engine.Put("key", "value");
auto value = engine.Get("key");
```

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

```bash
git checkout -b feature/my-feature
# Make changes, add tests
ctest --test-dir build -C Debug --output-on-failure
git commit -m "feat: Add feature X"
git push origin feature/my-feature
# Create pull request on GitHub
```

See [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) | [Code of Conduct](docs/CODE_OF_CONDUCT.md)

## License

MIT - See [LICENSE](LICENSE)

---

**[GitHub](https://github.com/Jameshunter1/VectorDBMS)**  **[Documentation](docs/DOCUMENTATION.md)**  **[Issues](https://github.com/Jameshunter1/VectorDBMS/issues)**
