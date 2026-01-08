# Vectis Vector Database - Complete Documentation

> **Production-Ready C++20 Page-Oriented Vector Database Engine**  
> Version 1.5 | Last Updated: January 2026

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [Building from Source](#building-from-source)
3. [Docker Deployment](#docker-deployment)
4. [API Reference](#api-reference)
5. [Performance](#performance)
6. [Security](#security)
7. [Contributing](#contributing)

---

## Quick Start

### Prerequisites
- **Windows**: Visual Studio 2022, CMake 3.20+
- **Linux/macOS**: GCC 10+/Clang 12+, CMake 3.20+
- **Docker**: Docker 20.10+ and Docker Compose (optional)

### Option 1: Local Build (5 minutes)

```powershell
# Windows
git clone https://github.com/Jameshunter1/VectorDBMS.git
cd VectorDBMS
cmake --preset windows-vs2022-x64-debug -S src
cmake --build build/windows-vs2022-x64-debug --config Debug
.\build\windows-vs2022-x64-debug\Debug\dbcli.exe
```

```bash
# Linux/macOS
git clone https://github.com/Jameshunter1/VectorDBMS.git
cd VectorDBMS
cmake -B build -S src -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/apps/dbcli
```

### Option 2: Docker (2 minutes)

```bash
# Start complete stack (database + monitoring)
docker compose up -d

# Access services
curl http://localhost:8080/api/stats    # Database API
open http://localhost:3000               # Grafana dashboard (admin/admin)
open http://localhost:9090               # Prometheus metrics
```

### First Commands

```bash
# Interactive CLI
dbcli> PUT greeting "Hello, Vectis!"
dbcli> GET greeting
dbcli> STATS
dbcli> EXIT

# HTTP API
curl -X POST "http://localhost:8080/api/put?key=test&value=data"
curl "http://localhost:8080/api/get?key=test"
curl "http://localhost:8080/api/stats"
```

---

## Building from Source

### Windows (Visual Studio)

```powershell
# Configure with CMake preset
cmake --preset windows-vs2022-x64-debug -S src

# Build all targets (library + apps + tests)
cmake --build build/windows-vs2022-x64-debug --config Debug

# Run tests
ctest --test-dir build/windows-vs2022-x64-debug -C Debug --output-on-failure

# Run specific executable
.\build\windows-vs2022-x64-debug\Debug\dbcli.exe
.\build\windows-vs2022-x64-debug\Debug\dbweb.exe --port 8080
```

### Linux/macOS (Make or Ninja)

```bash
# Configure
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCORE_ENGINE_BUILD_TESTS=ON

# Build (parallel)
cmake --build build -j$(nproc)

# Run tests
ctest --test-dir build --output-on-failure -j$(nproc)

# Install (optional)
sudo cmake --install build --prefix /usr/local
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `CORE_ENGINE_BUILD_TESTS` | ON | Build test suite |
| `CORE_ENGINE_BUILD_BENCHMARKS` | ON | Build performance benchmarks |
| `CMAKE_BUILD_TYPE` | Debug | Release, Debug, RelWithDebInfo |
| `CMAKE_CXX_FLAGS` | - | Additional compiler flags |

### Architecture-Specific Optimizations

```bash
# AVX2 optimization (Intel/AMD)
cmake -B build -S src -DCMAKE_CXX_FLAGS="-march=native -O3"

# ARM NEON optimization
cmake -B build -S src -DCMAKE_CXX_FLAGS="-march=armv8-a+simd -O3"
```

---

## Docker Deployment

### Linux Container (`Dockerfile`)

Use the default multi-stage Dockerfile when you want a single self-contained image (CI pipelines, registries, or lightweight dev/staging environments).

```bash
# Build image (dbweb + dbcli baked into /vectis)
docker build -t vectis:latest .

# Run database
docker run -d \
  --name vectis-db \
  -p 8080:8080 \
  -v vectis-data:/vectis/data \
  -e VECTIS_BUFFER_POOL_SIZE_MB=256 \
  vectis:latest

# Inspect
docker logs -f vectis-db
docker exec -it vectis-db ./dbcli
```

Key details:
- Health check hits `http://localhost:8080/api/health` every 30s using `wget`.
- Binaries live under `/vectis` and the server writes to `/vectis/data` (mounted by default).
- Best for Linux hosts that just need the database service without monitoring extras.

### Windows Container (`Dockerfile.windows`)

Run this when your deployment hosts require Windows Server Core or you need to validate NTFS + direct-I/O behavior.

```powershell
# Build (requires Windows container runtime)
docker build -t vectis-win -f Dockerfile.windows .

# Run
docker run -d `
  --name vectis-win `
  -p 8080:8080 `
  -v vectis-win-data:C:\vectis\data `
  vectis-win
```

MSVC toolchain is installed during the build stage, so expect a longer initial image build. Runtime entrypoint matches Linux (`dbweb.exe data_dir port`).

### Production Stack (`docker-compose.yml`)

Use Compose when you want Grafana dashboards and Prometheus metrics alongside the database.

```bash
# Bring up the stack (Vectis + Prometheus + Grafana)
docker compose up -d

# Tear down
docker compose down

# Tail logs or scale services
docker compose logs -f vectis
docker compose up -d --scale vectis=2
```

Compose defaults:
- `vectis` service builds from the local Dockerfile, persists data in the `vectis-data` volume, and exposes port 8080.
- `prometheus` loads scrapes from `monitoring/prometheus.yml` and stores TSDB data in `prometheus-data`.
- `grafana` boots with pre-provisioned dashboards/datasources under `monitoring/grafana-*` and stores state in `grafana-data`.

### Container Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `VECTIS_DATA_DIR` | `/vectis/data` (Linux) / `C:\\vectis\\data` (Windows) | Location for WAL + page files |
| `VECTIS_PORT` | `8080` | HTTP listener port for dbweb |
| `VECTIS_HOST` | `0.0.0.0` | Bind address (leave default inside containers) |
| `VECTIS_BUFFER_POOL_SIZE_MB` | `256` | Buffer pool size exposed via Compose example |
| `VECTIS_ENABLE_VECTOR_INDEX` | `true` | Enables ANN index initialization at startup |
| `VECTIS_VECTOR_DIMENSION` | `384` | Required DIM for inserted vectors (dbweb enforces this) |

Set these via `docker run -e KEY=value ...` or under the `environment:` block in `docker-compose.yml` to tailor deployments.

### Monitoring Setup

1. **Prometheus** scrapes `/metrics` endpoint every 15s
2. **Grafana** connects to Prometheus data source
3. **Pre-built dashboard** shows:
   - Request rate (ops/sec)
   - Latency (P50/P95/P99)
   - Page statistics (reads/writes)
   - Buffer pool hit rate
   - Active connections

**Access Grafana**: http://localhost:3000 (admin/admin)

---

## API Reference

### HTTP Endpoints

#### PUT - Store Key-Value Pair
```http
POST /api/put?key=<key>&value=<value>
```
Response: `200 OK` or `400/500` with error message

#### GET - Retrieve Value
```http
GET /api/get?key=<key>
```
Response: Value as plain text, or `404 NOT_FOUND`

#### DELETE - Remove Key
```http
POST /api/delete?key=<key>
```
Response: `200 OK` or `400/500` with error message

#### BATCH WRITE - Multiple Operations
```http
POST /api/batch
Content-Type: application/json

{
  "operations": [
    {"type": "PUT", "key": "k1", "value": "v1"},
    {"type": "DELETE", "key": "k2"}
  ]
}
```

#### SCAN - Range Query
```http
GET /api/scan?start=<start_key>&end=<end_key>&limit=100&reverse=false
```
Response: JSON array of key-value pairs

#### STATS - Database Metrics
```http
GET /api/stats
```
```json
{
  "total_pages": 1024,
  "total_puts": 50000,
  "total_gets": 125000,
  "avg_get_latency_us": 15.3,
  "buffer_pool_hit_rate": 0.982
}
```

#### METRICS - Prometheus Export
```http
GET /metrics
```
```
# Prometheus format (version 0.0.4)
core_engine_total_pages 1024
core_engine_total_reads 125000
core_engine_avg_get_latency_microseconds 15.3
```

### C++ API

```cpp
#include <core_engine/engine.hpp>

using namespace core_engine;

int main() {
  // Open database
  Engine engine;
  DatabaseConfig config = DatabaseConfig::Embedded("./my_db");
  config.buffer_pool_size = 1024;  // 4 MB cache
  engine.Open(config);

  // Basic operations
  engine.Put("key1", "value1");
  auto value = engine.Get("key1");  // std::optional<std::string>
  engine.Delete("key1");

  // Batch operations
  std::vector<Engine::BatchOperation> ops = {
    {Engine::BatchOperation::Type::PUT, "k1", "v1"},
    {Engine::BatchOperation::Type::DELETE, "k2"}
  };
  engine.BatchWrite(ops);

  // Range scan
  ScanOptions opts;
  opts.limit = 100;
  opts.reverse = false;
  auto results = engine.Scan("start_key", "end_key", opts);

  // Vector search (HNSW)
  config.enable_vector_index = true;
  config.vector_dimension = 128;
  vector::Vector vec({0.1f, 0.2f, /* 128 dims */});
  engine.PutVector("doc1", vec);
  auto similar = engine.SearchSimilar(vec, /*k=*/10);

  return 0;
}
```

---

## Performance

### Benchmarks (AMD Ryzen 9 / NVMe SSD)

| Operation | Throughput | Latency (P50) | Latency (P99) |
|-----------|------------|---------------|---------------|
| Sequential PUT | 180K ops/sec | 5.2 µs | 12 µs |
| Random GET (cached) | 850K ops/sec | 1.1 µs | 3.8 µs |
| Random GET (cold) | 25K ops/sec | 38 µs | 105 µs |
| Batch Write (100) | 220K ops/sec | 450 µs | 920 µs |
| Range Scan (1000) | 950K keys/sec | 1.0 ms | 2.8 ms |
| Vector Search (10K) | 12K queries/sec | 78 µs | 240 µs |

### Optimization Guide

**Buffer Pool Sizing**:
```cpp
config.buffer_pool_size = (RAM_GB * 0.25) * 256;  // 25% of RAM
// Example: 16 GB RAM → 1024 pages → 4 MB cache
```

**Compaction Tuning**:
```cpp
config.wal_sync_mode = WalSyncMode::kPeriodic;  // Balance durability vs speed
```

**SIMD Vectorization**:
```bash
# Enable AVX2/AVX-512 for distance calculations
cmake -DCMAKE_CXX_FLAGS="-march=native -O3"
```

### Known Limitations (Year 1)

- No distributed replication (single-node only)
- No column compression (raw page storage)
- No GPU acceleration (CPU-only HNSW)
- No query optimizer (simple sequential scans)

### Year 2: Async Disk Pipeline (Q1 2026)

- Linux builds now default to an io_uring-backed DiskManager when `liburing` is available.
- `DiskManager::ReadPagesBatch()` and `WritePagesBatch()` expose batched APIs used by buffer pool redo/flush paths.
- The io_uring queue depth is configurable via CMake option `-DCORE_ENGINE_ENABLE_IO_URING=ON|OFF` and `DiskManager::Options::io_uring_queue_depth`.
- Falls back to the portable synchronous path automatically on Windows/macOS or when `liburing` is missing.
- Tests cover batch semantics to guarantee identical correctness regardless of the backend.

---

## Security

### Authentication

```cpp
#include <core_engine/security/auth.hpp>

AuthManager auth;
auth.CreateUser("admin", "secure_password", {"admin"});
auth.CreateUser("readonly", "password", {"read"});

// Session management
std::string session_id = auth.CreateSession("admin", "127.0.0.1");
if (auth.ValidateSession(session_id)) {
  // User authenticated
}
```

### Audit Logging

```cpp
#include <core_engine/security/audit.hpp>

AuditLogger audit("./audit.log");
audit.LogLogin("alice", "192.168.1.100", true);
audit.LogPut("alice", "secret_key", true);
audit.LogUnauthorizedAccess("hacker", "203.0.113.1", "DELETE /admin");

// Query audit trail
auto recent = audit.GetRecentEntries(100);
auto failed_logins = audit.GetFailedLoginCount();
```

### Rate Limiting

```cpp
#include <core_engine/rate_limiter.hpp>

RateLimiter limiter(100.0, 200.0);  // 100/sec, burst 200
if (limiter.Allow("client_ip")) {
  // Process request
} else {
  // Return 429 Too Many Requests
}
```

### Best Practices

1. **TLS/HTTPS**: Use reverse proxy (nginx, Caddy) for HTTPS
2. **Network Isolation**: Run in private network, expose only via proxy
3. **Volume Encryption**: Use encrypted Docker volumes or LUKS
4. **Regular Backups**: Backup `_data/` directory hourly/daily
5. **Log Monitoring**: Send audit logs to SIEM (Splunk, ELK)

---

## Contributing

### Development Setup

```bash
# 1. Fork repository on GitHub
# 2. Clone your fork
git clone https://github.com/YOUR_USERNAME/VectorDBMS.git
cd VectorDBMS

# 3. Create feature branch
git checkout -b feature/my-improvement

# 4. Make changes and test
cmake --preset windows-vs2022-x64-debug -S src
cmake --build build/windows-vs2022-x64-debug --config Debug
ctest --test-dir build/windows-vs2022-x64-debug -C Debug

# 5. Commit with descriptive message
git add .
git commit -m "Add feature X: improves Y by Z%"

# 6. Push and create Pull Request
git push origin feature/my-improvement
```

### Code Style

- **Format**: LLVM style (2-space indent, 100-char line limit)
- **Auto-format**: Run `clang-format -i src/**/*.{cpp,hpp}`
- **Naming**:
  - Classes: `PascalCase` (Engine, BufferPoolManager)
  - Functions: `PascalCase` (GetPage, FlushAllPages)
  - Variables: `snake_case` (page_id, buffer_pool_size)
  - Private members: `trailing_underscore_` (disk_manager_, is_open_)
  - Constants: `kPascalCase` (kPageSize, kInvalidPageId)

### Testing Requirements

- All new features must include tests (Catch2 framework)
- Maintain 100% test pass rate
- Add benchmarks for performance-critical code
- Document public APIs with examples

### Pull Request Checklist

- [ ] Code follows LLVM style (clang-format)
- [ ] All tests pass (`ctest --output-on-failure`)
- [ ] Added tests for new functionality
- [ ] Updated documentation (this file)
- [ ] Commit messages are descriptive
- [ ] No merge conflicts with `main`
- [ ] Benchmarks show no performance regression

### Communication

- **Issues**: Use GitHub Issues for bugs/features
- **Discussions**: GitHub Discussions for questions
- **Security**: Email security@vectis.io for vulnerabilities
- **Code of Conduct**: Be respectful, inclusive, and professional

---

## License

MIT License - See [LICENSE](../LICENSE) file for details.

Copyright (c) 2026 James Hunter & Contributors

---

## Project Status

**Current Version**: 1.5 (January 2026)  
**Status**: Year 2 in progress (Linux io_uring disk pipeline shipped)  
**Next Milestone**: Year 2 Q2 - zero-copy buffer registration & IOCP parity

### Completed Milestones

✅ **Q1 2025**: Disk & Page Layer  
✅ **Q2 2025**: Buffer Pool Manager  
✅ **Q3 2025**: LRU-K Eviction Policy  
✅ **Q4 2025**: Write-Ahead Logging (ARIES)  
✅ **Q4 2025**: HNSW Vector Index  
✅ **Q1 2026**: Batch Operations & Metrics  
✅ **Q1 2026**: Docker Deployment & Monitoring  
✅ **Q1 2026**: Linux io_uring batched disk I/O

### Roadmap

- **2026 Q2**: Zero-copy buffer registration (io_uring fixed buffers + IOCP prototype)
- **2026 Q3**: Custom network protocol with connection pooling
- **2026 Q4**: Product quantization for vector compression
- **2027**: Multi-node replication and sharding
- **2028**: Managed SaaS offering

---

**Repository**: https://github.com/Jameshunter1/VectorDBMS  
**Documentation**: https://vectis.readthedocs.io  
**Discord**: https://discord.gg/vectis  
**Twitter**: @VectisDB  

**Built with ❤️ by the Vectis team**
