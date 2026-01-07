# Vectis Database - Build Guide

## 📁 Repository Structure

**Important:** Understanding the directory structure is crucial for successful builds.

```
VectorDBMS/                          ← Repository root (YOU ARE HERE)
├── .github/                         ← GitHub Actions, templates
├── build/                           ← Build artifacts (GENERATED - DO NOT EDIT)
│   ├── windows-vs2022-x64-debug/    ← Debug build with VS2022
│   ├── windows-msvc-debug/          ← Debug build with Ninja+MSVC
│   └── ...                          ← Other build configurations
├── src/                             ← Source code (CMake project root)
│   ├── CMakeLists.txt               ← Main build configuration
│   ├── CMakePresets.json            ← Build presets (configures ../build/)
│   ├── include/                     ← Public headers
│   ├── lib/                         ← Implementation files
│   └── apps/                        ← Executables (dbcli, dbweb, tutorial)
├── tests/                           ← Test suite (sibling to src/)
│   └── CMakeLists.txt               ← Test configuration
├── benchmarks/                      ← Performance benchmarks (sibling to src/)
│   └── CMakeLists.txt               ← Benchmark configuration
└── README.md                        ← You're looking at this
```

---

## 🚨 Critical Rules

### ❌ DO NOT DO THIS:
```powershell
# WRONG - Don't run CMake from src/ directory
cd src
cmake --preset windows-vs2022-x64-debug  # This creates src/build/ (BAD!)
```

### ✅ ALWAYS DO THIS:
```powershell
# CORRECT - Run CMake from repository root
cd VectorDBMS  # or cd c:\Users\James\SystemProjects\VectorDBMS\
cmake --preset windows-vs2022-x64-debug -S src  # This creates build/ at root (GOOD!)
```

**Why?**
- `CMakePresets.json` specifies `"binaryDir": "${sourceDir}/../build/${presetName}"`
- `${sourceDir}` = `src/` directory
- `../build/` = one level up = repository root `/build/`
- This keeps source directory clean and follows CMake best practices

---

## 🔧 Quick Start

### Prerequisites
- **Windows**: Visual Studio 2022 or MSVC Build Tools
- **CMake**: 3.24 or later
- **Git**: For version control

### 1. Clone Repository
```powershell
git clone https://github.com/Jameshunter1/VectorDBMS.git
cd VectorDBMS
```

### 2. Configure Build
```powershell
# Visual Studio 2022 (multi-config generator)
cmake --preset windows-vs2022-x64-debug -S src

# Or Ninja + MSVC (single-config generator)
cmake --preset windows-msvc-debug -S src
```

This creates: `build/windows-vs2022-x64-debug/` or `build/windows-msvc-debug/`

### 3. Build
```powershell
# Build everything (library, apps, tests, benchmarks)
cmake --build build/windows-vs2022-x64-debug --config Debug -j 8

# Or build specific targets
cmake --build build/windows-vs2022-x64-debug --config Debug --target core_engine
cmake --build build/windows-vs2022-x64-debug --config Debug --target dbweb
cmake --build build/windows-vs2022-x64-debug --config Debug --target core_engine_tests
```

### 4. Run Tests
```powershell
# Run all tests
ctest --test-dir build/windows-vs2022-x64-debug -C Debug --output-on-failure

# Run specific test
.\build\windows-vs2022-x64-debug\tests\Debug\core_engine_tests.exe "[Page]"
```

### 5. Run Application
```powershell
# Database web server
.\build\windows-vs2022-x64-debug\Debug\dbweb.exe

# CLI tool
.\build\windows-vs2022-x64-debug\Debug\dbcli.exe

# Tutorial
.\build\windows-vs2022-x64-debug\Debug\tutorial_v1.4.exe
```

---

## 🔄 Build Presets

CMake presets are defined in `src/CMakePresets.json`:

| Preset Name | Generator | Build Type | Test | Benchmark |
|-------------|-----------|------------|------|-----------|
| `windows-vs2022-x64-debug` | Visual Studio 17 2022 | Multi-config | ✅ | ✅ |
| `windows-vs2022-x64-release` | Visual Studio 17 2022 | Multi-config | ✅ | ❌ |
| `windows-msvc-debug` | Ninja | Debug | ✅ | ✅ |
| `windows-msvc-release` | Ninja | Release | ✅ | ❌ |

**Multi-config vs Single-config:**
- **Multi-config** (VS): One build directory contains both Debug and Release
  - Requires `-C Debug` or `-C Release` when running CTest
  - Executables in `Debug/` or `Release/` subdirectories
- **Single-config** (Ninja): Separate build directories for Debug/Release
  - No `-C` flag needed for CTest
  - Executables directly in build directory

---

## 🛠️ Advanced Build Options

### Clean Build
```powershell
# Remove build directory and start fresh
Remove-Item -Recurse -Force build/windows-vs2022-x64-debug
cmake --preset windows-vs2022-x64-debug -S src
cmake --build build/windows-vs2022-x64-debug --config Debug -j 8
```

### Build with Different Compilers
```powershell
# Clang-CL (if installed)
cmake -B build/clang -S src -G "Visual Studio 17 2022" -T ClangCL
cmake --build build/clang --config Debug

# MinGW (if installed)
cmake -B build/mingw -S src -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build/mingw
```

### Enable Sanitizers (for debugging)
```powershell
cmake --preset windows-msvc-debug -S src -DCORE_ENGINE_ENABLE_SANITIZERS=ON
cmake --build build/windows-msvc-debug -j 8
```

### Disable Tests/Benchmarks
```powershell
cmake --preset windows-vs2022-x64-release -S src \
  -DCORE_ENGINE_BUILD_TESTS=OFF \
  -DCORE_ENGINE_BUILD_BENCHMARKS=OFF
```

---

## 📊 Benchmarks

### Run All Benchmarks
```powershell
.\build\windows-vs2022-x64-debug\benchmarks\Debug\core_engine_benchmarks.exe
```

### Run Specific Benchmarks
```powershell
# Storage layer only
.\build\windows-vs2022-x64-debug\benchmarks\Debug\core_engine_benchmarks.exe --benchmark_filter=BM_Page.*

# Vector operations only
.\build\windows-vs2022-x64-debug\benchmarks\Debug\core_engine_benchmarks.exe --benchmark_filter=BM_Vector.*

# Export results
.\build\windows-vs2022-x64-debug\benchmarks\Debug\core_engine_benchmarks.exe --benchmark_format=json > results.json
```

---

## 🧪 Testing

### Run All Tests
```powershell
ctest --test-dir build/windows-vs2022-x64-debug -C Debug --output-on-failure
```

### Run Tests by Category
```powershell
# Storage layer tests
ctest --test-dir build/windows-vs2022-x64-debug -C Debug -R "Storage"

# Engine tests
ctest --test-dir build/windows-vs2022-x64-debug -C Debug -R "Engine"

# Web API tests
ctest --test-dir build/windows-vs2022-x64-debug -C Debug -R "Web"
```

### Run Standalone Test Executables
```powershell
# Core tests (Catch2)
.\build\windows-vs2022-x64-debug\tests\Debug\core_engine_tests.exe

# Security tests
.\build\windows-vs2022-x64-debug\tests\Debug\security_tests.exe

# Advanced features
.\build\windows-vs2022-x64-debug\tests\Debug\advanced_tests.exe
```

---

## 🐧 Linux/macOS Build

### Configure and Build
```bash
# From repository root
cd VectorDBMS

# Configure with Ninja (recommended)
cmake -B build -S src -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCORE_ENGINE_BUILD_TESTS=ON \
  -DCORE_ENGINE_BUILD_BENCHMARKS=ON

# Or with Unix Makefiles
cmake -B build -S src -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build -j$(nproc)

# Run tests (no -C flag needed)
ctest --test-dir build --output-on-failure

# Run application
./build/dbweb
```

---

## 🐛 Troubleshooting

### Problem: "CMake Error: Could not find CMAKE_ROOT"
**Solution:** Make sure you're running from repository root:
```powershell
cd c:\Users\James\SystemProjects\VectorDBMS
cmake --preset windows-vs2022-x64-debug -S src
```

### Problem: "build/ directory has wrong structure"
**Solution:** You may have run CMake from the wrong directory. Clean and rebuild:
```powershell
Remove-Item -Recurse -Force build, src/build
cmake --preset windows-vs2022-x64-debug -S src
```

### Problem: Tests fail with "cannot find executable"
**Solution:** Multi-config generators put executables in Debug/ or Release/ subdirs:
```powershell
# Multi-config: Specify configuration
ctest --test-dir build/windows-vs2022-x64-debug -C Debug

# Single-config: No -C flag needed
ctest --test-dir build/windows-msvc-debug
```

### Problem: "File not found: src/CMakeLists.txt"
**Solution:** You're running CMake from the wrong directory:
```powershell
# Check current directory
Get-Location

# Should be repository root
cd c:\Users\James\SystemProjects\VectorDBMS

# Then run CMake
cmake --preset windows-vs2022-x64-debug -S src
```

---

## 📚 Additional Resources

- **Quick Start Guide**: [QUICK_START_v1.5.md](QUICK_START_v1.5.md)
- **System Improvements**: [SYSTEM_IMPROVEMENTS_v1.5.md](SYSTEM_IMPROVEMENTS_v1.5.md)
- **API Reference**: [src/API_REFERENCE.md](src/API_REFERENCE.md)
- **Architecture**: [.github/copilot-instructions.md](.github/copilot-instructions.md)

---

## 🎯 Summary

**Golden Rules:**
1. ✅ Always run CMake from **repository root**
2. ✅ Use `-S src` to specify source directory
3. ✅ Build artifacts go in `build/` at repository root
4. ❌ Never create builds in `src/build/` directory
5. ✅ Multi-config generators need `-C Debug` for CTest

**Quick Reference:**
```powershell
# Configure
cmake --preset windows-vs2022-x64-debug -S src

# Build
cmake --build build/windows-vs2022-x64-debug --config Debug -j 8

# Test
ctest --test-dir build/windows-vs2022-x64-debug -C Debug --output-on-failure

# Run
.\build\windows-vs2022-x64-debug\Debug\dbweb.exe
```

---

**Happy Building! 🚀**
