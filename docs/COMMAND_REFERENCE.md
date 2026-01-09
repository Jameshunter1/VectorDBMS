# VectorDBMS Command Reference

Complete reference of all commands used in the VectorDBMS project for building, testing, formatting, and deployment.

---

## 🔧 Build Commands

### Configure CMake
```powershell
# Configure the project with CMake (creates build files)
# Use debug preset (recommended for development)
cmake --preset=windows-vs2022-x64-debug

# Or use release preset (optimized build)
cmake --preset=windows-vs2022-x64-release
```

### Build the Project
```powershell
# Build all targets (Debug)
cmake --build build/windows-vs2022-x64-debug

# Build all targets (Release)
cmake --build build/windows-vs2022-x64-release

# Build specific target (Debug)
cmake --build build/windows-vs2022-x64-debug --target dbcli
cmake --build build/windows-vs2022-x64-debug --target dbweb
cmake --build build/windows-vs2022-x64-debug --target core_engine_tests
```

### Clean Build
```powershell
# Remove build directory and rebuild from scratch
Remove-Item -Recurse -Force build
cmake --preset=windows-vs2022-x64-debug
cmake --build build/windows-vs2022-x64-debug
```

---

## 🧪 Testing Commands

### Run All Tests
```powershell
# Run all tests with CTest (from project root)
cd build/windows-vs2022-x64-debug
ctest --output-on-failure -C Debug
cd ../..

# Or run directly (Debug build)
.\build\windows-vs2022-x64-debug\tests\Debug\core_engine_tests.exe
```

### Run Specific Tests
```powershell
# Run specific test executable (Debug)
.\build\windows-vs2022-x64-debug\tests\Debug\core_engine_tests.exe

# Run with verbose output
.\build\windows-vs2022-x64-debug\tests\Debug\core_engine_tests.exe --verbose
```

---

## 🎨 Code Formatting Commands

### Format All Code
```powershell
# Format all C++ source files using clang-format
.\format.ps1
```

### Check Formatting (CI Validation)
```powershell
# Check if all files are properly formatted (doesn't modify files)
.\format-check.ps1
```

### Manual Format Single File
```powershell
# Format a specific file
clang-format -i -style=file path\to\file.cpp

# Check formatting without modifying
clang-format --dry-run --Werror -style=file path\to\file.cpp
```

---

## 🚀 Running Applications

### Database CLI (dbcli)
```powershell
# Run the database CLI (Debug)
.\build\windows-vs2022-x64-debug\apps\dbcli\Debug\dbcli.exe

# With specific database path
.\build\windows-vs2022-x64-debug\apps\dbcli\Debug\dbcli.exe --db-path=./mydb.db
```

### Database Web Server (dbweb)
```powershell
# Run the web server (default port 8080) - Debug
.\build\windows-vs2022-x64-debug\apps\dbweb\Debug\dbweb.exe

# With custom port
.\build\windows-vs2022-x64-debug\apps\dbweb\Debug\dbweb.exe --port=9000

# Stop the server (if running in background)
# Find process ID first
Get-Process | Where-Object {$_.ProcessName -like "*dbweb*"}
Stop-Process -Id <PID>
```

---

## 📊 Benchmarks

### Run Benchmarks
```powershell
# Run all benchmarks (Debug build)
.\build\windows-vs2022-x64-debug\benchmarks\Debug\bench_advanced.exe
.\build\windows-vs2022-x64-debug\benchmarks\Debug\bench_storage_layer.exe
.\build\windows-vs2022-x64-debug\benchmarks\Debug\bench_vector_ops.exe
.\build\windows-vs2022-x64-debug\benchmarks\Debug\bench_page_file.exe
```

---

## 📝 Demo Scripts

### Vector Operations Demo
```powershell
# Simple vector demo
.\scripts\demo_vectors_simple.ps1

# Full vector demo with HNSW index
.\scripts\demo_vectors.ps1
```

### Generate Test Data
```powershell
# Generate test data for benchmarks
.\scripts\generate_test_data.ps1
```

---

## 🔍 Git Commands

### Check Status
```powershell
# View current git status
git status

# View commit history
git log --oneline -10
```

### Stage and Commit
```powershell
# Stage all changes
git add -A

# Stage specific files
git add path\to\file.cpp

# Commit with message
git commit -m "your commit message"
```

### Push and Pull
```powershell
# Push to remote
git push

# Pull from remote
git pull

# Push specific branch
git push origin main
```

### View Differences
```powershell
# View unstaged changes
git diff

# View staged changes
git diff --cached

# View changes in specific file
git diff path\to\file.cpp
```

---

## 🛠️ Development Workflow

### Complete Build and Test Cycle
```powershell
# 1. Format code
.\format.ps1

# 2. Build project (Debug)
cmake --build build/windows-vs2022-x64-debug

# 3. Run tests
cd build/windows-vs2022-x64-debug
ctest --output-on-failure -C Debug
cd ../..

# 4. Commit and push
git add -A
git commit -m "your message"
git push
```

### Quick Rebuild After Code Changes
```powershell
# Format, build, and test (Debug)
.\format.ps1
cmake --build build/windows-vs2022-x64-debug
.\build\windows-vs2022-x64-debug\tests\Debug\core_engine_tests.exe
```

---

## 🐛 Debugging Commands

### Verbose Build Output
```powershell
# Build with verbose output to see compilation commands
cmake --build build/windows-vs2022-x64-debug --verbose
```

### Check Build Logs
```powershell
# View build output (if redirected to file)
Get-Content build_log.txt -Tail 50
```

### Find Processes
```powershell
# Find running database processes
Get-Process | Where-Object {$_.ProcessName -like "*db*"}

# Kill specific process
Stop-Process -Id <PID>
Stop-Process -Name dbweb
```

---

## 📦 CMake Presets

### Available Presets
```powershell
# List all available CMake presets
cmake --list-presets

# Use debug preset (recommended for development)
cmake --preset=windows-vs2022-x64-debug

# Use release preset (optimized build)
cmake --preset=windows-vs2022-x64-release
```

---

## 🔧 Clang-Format Configuration

### Current Configuration
The project uses `.clang-format` with these settings:
- **BasedOnStyle**: LLVM
- **IndentWidth**: 2
- **ColumnLimit**: 100
- **PointerAlignment**: Left
- **AllowShortFunctionsOnASingleLine**: None
- **SortIncludes**: true

### Check Clang-Format Version
```powershell
# Check installed version
clang-format --version

# Try different versions
clang-format-17 --version
clang-format-16 --version
```

---

## 📁 Project Structure

### Key Directories
```
VectorDBMS/
├── src/
│   ├── include/        # Header files
│   ├── lib/           # Implementation files
│   └── apps/          # Applications (dbcli, dbweb)
├── tests/             # Test files
├── benchmarks/        # Benchmark files
├── scripts/           # PowerShell scripts
├── build/             # Build output (generated)
│   └── windows-vs2022-x64-debug/    # Debug build directory
│       ├── apps/
│       │   ├── dbcli/Debug/         # dbcli.exe
│       │   └── dbweb/Debug/         # dbweb.exe
│       ├── tests/Debug/             # core_engine_tests.exe
│       └── benchmarks/Debug/        # Benchmark executables
└── .github/           # GitHub configuration
```

> [!NOTE]
> **Windows VS2022 Build Structure**: The Visual Studio generator creates a nested directory structure with `Debug/` and `Release/` subdirectories. Executables are located in these subdirectories, not directly in the target folders.

---

## 🚨 Common Issues and Solutions

### Issue: Build Fails
```powershell
# Solution: Clean rebuild
Remove-Item -Recurse -Force build
cmake --preset=windows-vs2022-x64-debug
cmake --build build/windows-vs2022-x64-debug
```

### Issue: Formatting Errors
```powershell
# Solution: Run formatter
.\format.ps1

# Check what needs formatting
.\format-check.ps1
```

### Issue: Tests Fail
```powershell
# Solution: Rebuild and run with verbose output
cmake --build build/windows-vs2022-x64-debug
.\build\windows-vs2022-x64-debug\tests\Debug\core_engine_tests.exe --verbose
```

### Issue: Port Already in Use (dbweb)
```powershell
# Solution: Find and kill process using port
Get-Process | Where-Object {$_.ProcessName -like "*dbweb*"}
Stop-Process -Name dbweb

# Or use different port
.\build\windows-vs2022-x64-debug\apps\dbweb\Debug\dbweb.exe --port=9000
```

---

## 📚 Quick Reference

### Most Common Commands
```powershell
# Daily workflow (Debug build)
.\format.ps1                                                              # Format code
cmake --build build/windows-vs2022-x64-debug                             # Build
.\build\windows-vs2022-x64-debug\tests\Debug\core_engine_tests.exe      # Test
git add -A && git commit -m "msg"                                        # Commit
git push                                                                  # Push

# Check everything
.\format-check.ps1                                                        # Check formatting
git status                                                                # Check git status
cd build/windows-vs2022-x64-debug && ctest --output-on-failure -C Debug  # Run all tests
```

### Environment Setup
```powershell
# Install dependencies (one-time setup)
winget install LLVM.LLVM                 # For clang-format
winget install Kitware.CMake             # For CMake
winget install Git.Git                   # For Git
```

---

## 💡 Tips

1. **Always format before committing**: Run `.\format.ps1` before `git commit`
2. **Use correct presets**: Use `cmake --preset=windows-vs2022-x64-debug` for development
3. **Incremental builds**: CMake automatically detects changes, just run `cmake --build build/windows-vs2022-x64-debug`
4. **Test frequently**: Run tests after significant changes
5. **Clean build when stuck**: Remove `build/` directory and rebuild from scratch
6. **Remember Debug subdirectory**: Executables are in `build/windows-vs2022-x64-debug/.../Debug/` not directly in target folders

---

*Last updated: 2026-01-09*
