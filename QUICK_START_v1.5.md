# Vectis Database - Quick Start Guide

## 🚀 Getting Started with v1.5 Features

### Prerequisites
- Windows 10/11 with Visual Studio 2022
- CMake 3.24+
- C++20 compatible compiler

### Build Everything
```powershell
# Configure project
cd src
cmake --preset windows-vs2022-x64-debug

# Build all targets (library, apps, tests, benchmarks)
cmake --build build/windows-vs2022-x64-debug --config Debug -j 8
```

---

## 🧪 Running Tests

### Run All Tests
```powershell
ctest --test-dir build/windows-vs2022-x64-debug -C Debug --output-on-failure
```

### Run Specific Test Categories
```powershell
# Storage layer tests only
ctest --test-dir build/windows-vs2022-x64-debug -C Debug -R "Storage"

# Web API tests only
ctest --test-dir build/windows-vs2022-x64-debug -C Debug -R "Web"

# Engine tests only
ctest --test-dir build/windows-vs2022-x64-debug -C Debug -R "Engine"
```

### Run Standalone Test Executables
```powershell
# Core tests (Catch2-based)
.\build\windows-vs2022-x64-debug\tests\Debug\core_engine_tests.exe

# Security tests
.\build\windows-vs2022-x64-debug\tests\Debug\security_tests.exe

# Advanced features tests
.\build\windows-vs2022-x64-debug\tests\Debug\advanced_tests.exe
```

---

## 📊 Running Benchmarks

### Run All Benchmarks
```powershell
.\build\windows-vs2022-x64-debug\benchmarks\Debug\core_engine_benchmarks.exe
```

### Run Specific Benchmark Categories
```powershell
# Storage layer only
.\build\windows-vs2022-x64-debug\benchmarks\Debug\core_engine_benchmarks.exe --benchmark_filter=BM_Page.*

# DiskManager only
.\build\windows-vs2022-x64-debug\benchmarks\Debug\core_engine_benchmarks.exe --benchmark_filter=BM_DiskManager.*

# Vector operations only
.\build\windows-vs2022-x64-debug\benchmarks\Debug\core_engine_benchmarks.exe --benchmark_filter=BM_Vector.*

# HNSW index only
.\build\windows-vs2022-x64-debug\benchmarks\Debug\core_engine_benchmarks.exe --benchmark_filter=BM_HNSW.*
```

### Benchmark Output Format Options
```powershell
# JSON output
.\build\windows-vs2022-x64-debug\benchmarks\Debug\core_engine_benchmarks.exe --benchmark_format=json > results.json

# CSV output
.\build\windows-vs2022-x64-debug\benchmarks\Debug\core_engine_benchmarks.exe --benchmark_format=csv > results.csv

# Console output with details
.\build\windows-vs2022-x64-debug\benchmarks\Debug\core_engine_benchmarks.exe --benchmark_counters_tabular=true
```

---

## 🎨 Using the Dashboard UI

### 1. Start the Database Server
```powershell
# Build the server if not already built
cmake --build build/windows-vs2022-x64-debug --config Debug --target dbweb

# Run the server (default port 8080)
.\build\windows-vs2022-x64-debug\Debug\dbweb.exe
```

The server will output:
```
[INFO] Engine opened (Year 1 Q4 - Write-Ahead Logging + LRU-K Buffer Pool)
[INFO] Server listening on http://127.0.0.1:8080
```

### 2. Open the Dashboard
**Option A: Double-click the HTML file**
```powershell
start src\apps\dbweb\dashboard.html
```

**Option B: Open in browser**
Navigate to: `file:///c:/Users/YOUR_USERNAME/SystemProjects/VectorDBMS/src/apps/dbweb/dashboard.html`

### 3. Verify Connection
The dashboard will automatically connect to `http://127.0.0.1:8080/api` and display:
- ✅ Green pulsing status indicator (top-left)
- Real-time statistics in header
- Console log: `[SUCCESS] Connected to database`

### 4. Basic Operations

**Insert Data:**
1. Go to **Operations** tab
2. Enter key: `user_123`
3. Enter value: `{"name":"Alice","age":30,"email":"alice@example.com"}`
4. Click **PUT**
5. Console shows: `[timestamp] ✓ PUT "user_123"`

**Query Data:**
1. Enter key: `user_123`
2. Click **GET**
3. Value field populates with stored data

**Browse Data:**
1. Go to **Data Browser** tab
2. Click **🔄 Refresh** to load all entries
3. Use search box to filter
4. Click **View** to load entry into Operations tab
5. Click **Delete** to remove entry

**Generate Test Data:**
1. Go to **Operations** tab
2. Click **Generate Test Data**
3. Inserts 1000 entries (`test_0` through `test_999`)
4. Progress logs in console

**Monitor Performance:**
1. Go to **Performance** tab
2. View storage metrics (pages, cache hit rate)
3. View operations/sec (reads, writes)
4. Metrics auto-refresh every 5 seconds

---

## 🔧 Development Workflow

### Test-Driven Development
```powershell
# 1. Write test in tests/test_*.cpp
# 2. Build tests
cmake --build build/windows-vs2022-x64-debug --config Debug --target core_engine_tests

# 3. Run specific test
ctest --test-dir build/windows-vs2022-x64-debug -C Debug -R "YourTestName" --output-on-failure

# 4. Iterate until passing
```

### Performance Tuning
```powershell
# 1. Establish baseline
.\build\windows-vs2022-x64-debug\benchmarks\Debug\core_engine_benchmarks.exe --benchmark_filter=BM_YourBenchmark > before.txt

# 2. Make optimization changes

# 3. Rebuild
cmake --build build/windows-vs2022-x64-debug --config Debug --target core_engine_benchmarks

# 4. Run benchmark again
.\build\windows-vs2022-x64-debug\benchmarks\Debug\core_engine_benchmarks.exe --benchmark_filter=BM_YourBenchmark > after.txt

# 5. Compare results
compare.py before.txt after.txt  # Google Benchmark compare.py script
```

### UI Development
```powershell
# 1. Start server
.\build\windows-vs2022-x64-debug\Debug\dbweb.exe

# 2. Edit dashboard.html in your favorite editor

# 3. Refresh browser (Ctrl+R) to see changes

# 4. Use browser DevTools (F12) for debugging:
#    - Console: View JavaScript errors
#    - Network: Monitor API calls
#    - Elements: Inspect DOM
```

---

## 📈 Performance Expectations

### Typical Benchmark Results (NVMe SSD, i7-12700K)

**Storage Layer:**
```
BM_Page_ComputeChecksum         500 ns/iter (2M pages/sec)
BM_DiskManager_SequentialWrite  2 µs/iter (500K IOPS)
BM_DiskManager_RandomRead       5 µs/iter (200K IOPS)
BM_BufferPool_CacheHit          100 ns/iter (10M ops/sec)
BM_BufferPool_CacheMiss         10 µs/iter (100K ops/sec)
```

**Vector Operations:**
```
BM_VectorDistance_Cosine/128    0.5 µs/iter (2M vectors/sec)
BM_HNSW_Insert                  2 ms/iter (500 inserts/sec)
BM_HNSW_Search_K/10             5 ms/iter (200 queries/sec)
BM_Engine_SearchSimilar         10 ms/iter (100 queries/sec)
```

**Dashboard Responsiveness:**
- Single PUT/GET: <50ms (including network round-trip)
- Batch insert 1000 entries: 5-10 seconds
- Stats refresh: <100ms
- Data browser load (1000 entries): <500ms

---

## 🐛 Troubleshooting

### Tests Fail with Timeout
**Problem:** Tests run for >60 seconds and fail
**Solution:** Tests may be using non-temporary directories that persist data
```powershell
# Clean test directories
Remove-Item -Recurse -Force $env:TEMP\core_engine_test_*
```

### Dashboard Shows "Connection Error"
**Problem:** Can't connect to API at http://127.0.0.1:8080
**Solutions:**
1. Verify server is running: `netstat -an | findstr 8080`
2. Check firewall isn't blocking port 8080
3. Edit `dashboard.html` and change `API_BASE` if server uses different port

### Benchmarks Crash with Access Violation
**Problem:** Benchmark executable crashes
**Solution:** Ensure test directories are cleaned up
```powershell
# Clean benchmark directories
Remove-Item -Recurse -Force $env:TEMP\bench_*
```

### Build Fails with "main() already defined"
**Problem:** Multiple `main()` functions in benchmark files
**Solution:** Only `bench_page_file.cpp` should have `BENCHMARK_MAIN()`. Others should only register benchmarks with `BENCHMARK(Name)`.

---

## 📚 Additional Resources

- **API Reference:** [src/API_REFERENCE.md](src/API_REFERENCE.md)
- **Build Instructions:** [src/BUILDING.md](src/BUILDING.md)
- **Architecture Overview:** [.github/copilot-instructions.md](.github/copilot-instructions.md)
- **System Improvements:** [SYSTEM_IMPROVEMENTS_v1.5.md](SYSTEM_IMPROVEMENTS_v1.5.md)
- **Security Features:** [src/SECURITY.md](src/SECURITY.md)

---

## 🎯 Next Steps

1. **Explore Examples:**
   ```powershell
   .\build\windows-vs2022-x64-debug\Debug\tutorial_v1.4.exe
   ```

2. **Run Production Server:**
   ```powershell
   # Use production config
   .\build\windows-vs2022-x64-debug\Debug\dbweb.exe --config production.yaml --port 443
   ```

3. **Integrate with Your Application:**
   ```cpp
   #include <core_engine/engine.hpp>
   
   core_engine::DatabaseConfig config = core_engine::DatabaseConfig::Embedded("./myapp_db");
   core_engine::Engine engine;
   engine.Open(config);
   
   // Your code here
   ```

4. **Contribute:**
   - Check [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines
   - Report bugs at [GitHub Issues](https://github.com/Jameshunter1/VectorDBMS/issues)
   - Submit PRs with tests + benchmarks

---

**Happy Coding! 🚀**

*For questions or support, open an issue on GitHub.*
