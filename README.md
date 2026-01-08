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
cmake --preset windows-vs2022-x64-debug
cmake --build build/windows-vs2022-x64-debug --config Debug
.\build\windows-vs2022-x64-debug\Debug\dbcli.exe

# Local build - Linux/macOS
git clone https://github.com/Jameshunter1/VectorDBMS.git && cd VectorDBMS
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/dbcli
```

> **Note (Linux)**: Install `liburing-dev` to enable the asynchronous disk pipeline. Disable it via `-DCORE_ENGINE_ENABLE_IO_URING=OFF` if building on distributions without liburing.

### Developer Utilities

- **dbcli** – interactive CLI for day-to-day key/value + vector ops.
- **dbweb** – modern dashboard with persistent Browse Data, bulk vector loader, and live stats.
- **disk_demo** – DiskManager milestone sample that performs single-page and contiguous I/O round-trips.

```bash
# Build and run the disk demo (creates ./_disk_demo/disk_demo.db by default)
cmake --build build --target disk_demo
./build/apps/examples/disk_demo ./_disk_demo 8 32
```

### Container Targets

- **Dockerfile (Linux)**: Multi-stage build that produces a minimal Ubuntu 22.04 image with `dbweb` and `dbcli`. Use this when you need to bake application changes, ship to registries, or run a single container without Prometheus/Grafana.
- **Dockerfile.windows**: Windows Server Core container that compiles with MSVC. Use when your production hosts only support Windows containers or you need to validate Windows-specific filesystem behavior.
- **docker-compose.yml**: Spins up the full stack (Vectis + Prometheus + Grafana) with persistent volumes. Ideal for demos, monitoring, or local ops teams that want metrics/visuals out-of-the-box.

Each artifact shares the same defaults (`/vectis/data` volume, port `8080`, health check against `/api/health`). See [docs/DOCUMENTATION.md](docs/DOCUMENTATION.md#docker-deployment) for advanced flags and environment variables.

## Features

 **Page-based storage** with WAL recovery  
 **LRU-K buffer pool** with O(log N) eviction  
 **io_uring disk I/O** with batched read/write pipelines (Linux)  
 **HNSW vector index** for similarity search  
 **Batch operations** and range scans  
 **Prometheus metrics** + Grafana dashboards  
 **Vector-aware web console** with server-backed Browse Data + bulk loader  
 **Docker deployment** with monitoring stack  

## Performance

| Operation | Throughput | Latency |
|-----------|------------|---------|
| PUT | 180K/sec | 5.2 us |
| GET (cached) | 850K/sec | 1.1 us |
| Vector Search | 12K/sec | 78 us |

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
- Disk & Page Layer · Buffer Pool · LRU-K · WAL · HNSW · Metrics

**Year 2 (2026)**:
- ✅ Q1: Linux io_uring batch I/O + DiskManager batching APIs
- 🔜 Q2: Zero-copy buffer registration & IOCP parity on Windows

**Roadmap**:
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
