# Building VectorDBMS

This guide provides detailed instructions for building VectorDBMS on different platforms.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Quick Start](#quick-start)
- [Platform-Specific Instructions](#platform-specific-instructions)
  - [Windows](#windows)
  - [Linux](#linux)
  - [macOS](#macos)
- [Build Options](#build-options)
- [CMake Presets](#cmake-presets)
- [IDE Integration](#ide-integration)
- [Troubleshooting](#troubleshooting)

## Prerequisites

### Required

- **CMake**: 3.20 or higher
- **C++ Compiler**: Supporting C++20
  - Windows: MSVC 2022 (Visual Studio 17.0+)
  - Linux: GCC 11+ or Clang 13+
  - macOS: AppleClang 13+ (Xcode 13+)
- **Git**: For cloning the repository

### Optional

- **Ninja**: For faster builds (recommended)
- **Doxygen**: For building documentation
- **Google Test**: Automatically fetched by CMake
- **Google Benchmark**: Automatically fetched by CMake

## Quick Start

```bash
# Clone the repository
git clone https://github.com/Jameshunter1/VectorDBMS.git
cd VectorDBMS

# Configure and build (choose your platform)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Run tests
cd build
ctest --output-on-failure

# Run the CLI
./dbcli

# Run the web interface
./dbweb
# Then open http://localhost:8080
```

## Platform-Specific Instructions

### Windows

#### Option 1: Visual Studio 2022 (Recommended)

```powershell
# Using CMake Presets
cmake --preset windows-vs2022-x64-debug
cmake --build --preset windows-vs2022-x64-debug

# Or manually
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

#### Option 2: Visual Studio IDE

1. Open Visual Studio 2022
2. File → Open → CMake → Select `CMakeLists.txt`
3. Select your configuration (Debug/Release)
4. Build → Build All

#### Option 3: Command Line with Ninja

```powershell
# Install Ninja via chocolatey
choco install ninja

# Configure and build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Linux

#### Ubuntu/Debian

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build git

# GCC 11+ (Ubuntu 22.04+)
sudo apt-get install -y gcc-11 g++-11

# Or Clang 15+
sudo apt-get install -y clang-15 clang++-15

# Configure and build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

#### Fedora/RHEL

```bash
# Install dependencies
sudo dnf install -y gcc gcc-c++ cmake ninja-build git

# Configure and build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

#### Arch Linux

```bash
# Install dependencies
sudo pacman -S base-devel cmake ninja git

# Configure and build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### macOS

```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install CMake (via Homebrew)
brew install cmake ninja

# Configure and build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)
```

## Build Options

Configure the build with CMake options:

```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DBUILD_BENCHMARKS=ON \
  -DBUILD_EXAMPLES=ON \
  -DBUILD_DOCUMENTATION=OFF
```

### Available Options

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | `Release` | Build type: Debug, Release, RelWithDebInfo, MinSizeRel |
| `BUILD_TESTING` | `ON` | Build test suite |
| `BUILD_BENCHMARKS` | `ON` | Build performance benchmarks |
| `BUILD_EXAMPLES` | `ON` | Build example applications |
| `BUILD_DOCUMENTATION` | `OFF` | Build Doxygen documentation |
| `ENABLE_SANITIZERS` | `OFF` | Enable sanitizers (ASan, UBSan, etc.) |
| `ENABLE_LTO` | `OFF` | Enable Link-Time Optimization |

### Build Types

- **Debug**: No optimization, full debug info (`-O0 -g`)
- **Release**: Full optimization, no debug info (`-O3 -DNDEBUG`)
- **RelWithDebInfo**: Optimized with debug info (`-O2 -g`)
- **MinSizeRel**: Optimize for size (`-Os`)

## CMake Presets

We provide CMake presets for common configurations:

```bash
# List available presets
cmake --list-presets

# Use a preset
cmake --preset windows-vs2022-x64-debug
cmake --build --preset windows-vs2022-x64-debug
```

### Available Presets

- `windows-vs2022-x64-debug`: Windows, Visual Studio 2022, Debug
- `windows-vs2022-x64-release`: Windows, Visual Studio 2022, Release
- `windows-msvc-debug`: Windows, MSVC command-line, Debug
- `linux-gcc-debug`: Linux, GCC, Debug
- `linux-gcc-release`: Linux, GCC, Release
- `linux-clang-debug`: Linux, Clang, Debug
- `macos-debug`: macOS, AppleClang, Debug
- `macos-release`: macOS, AppleClang, Release

## IDE Integration

### Visual Studio Code

1. Install CMake Tools extension
2. Open the project folder
3. Select a CMake preset or configure manually
4. Press F7 to build

### CLion

1. Open the project folder
2. CLion will automatically detect CMake
3. Select your build configuration
4. Build → Build Project

### Visual Studio 2022

1. Open the folder containing `CMakeLists.txt`
2. Visual Studio will configure CMake automatically
3. Select your configuration from the dropdown
4. Build → Build All

## Advanced Build Configurations

### With Sanitizers

```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build
```

### With Link-Time Optimization (LTO)

```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
cmake --build build
```

### Static Analysis

```bash
# Generate compile_commands.json
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Run clang-tidy
clang-tidy -p build lib/*.cpp
```

### Cross-Compilation

```bash
# Example: Cross-compile for ARM64
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Troubleshooting

### CMake Cannot Find Compiler

**Problem**: CMake fails to detect your compiler.

**Solution**:
```bash
# Specify compiler explicitly
cmake -B build -DCMAKE_C_COMPILER=gcc-11 -DCMAKE_CXX_COMPILER=g++-11
```

### Missing Dependencies

**Problem**: Build fails due to missing dependencies.

**Solution**: Dependencies are fetched automatically via CMake FetchContent. Ensure you have internet connectivity during the first build.

### Build Errors on Windows

**Problem**: Build fails with "cannot open file" errors.

**Solution**: Run Visual Studio as Administrator or check antivirus settings.

### Out of Memory During Build

**Problem**: Linker runs out of memory.

**Solution**:
```bash
# Reduce parallel jobs
cmake --build build -j2  # Instead of -j
```

### Tests Fail

**Problem**: Tests fail after building.

**Solution**:
```bash
# Run tests with verbose output
cd build
ctest --output-on-failure --verbose
```

### Slow Build Times

**Problem**: Build takes too long.

**Solution**:
- Use Ninja instead of Make
- Enable parallel builds: `cmake --build build -j`
- Use ccache: `cmake -B build -DCMAKE_CXX_COMPILER_LAUNCHER=ccache`

## Performance Tips

1. **Use Ninja**: Faster than Make or MSBuild
   ```bash
   cmake -B build -G Ninja
   ```

2. **Parallel Builds**: Use all CPU cores
   ```bash
   cmake --build build -j$(nproc)  # Linux/macOS
   cmake --build build -j%NUMBER_OF_PROCESSORS%  # Windows
   ```

3. **ccache**: Cache compilation results
   ```bash
   sudo apt-get install ccache  # Linux
   brew install ccache          # macOS
   cmake -B build -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
   ```

4. **Precompiled Headers**: Automatically enabled for large projects

## Next Steps

After building:

- Run the [tests](../tests/README.md)
- Try the [examples](../apps/examples/README.md)
- Read the [API documentation](../API_REFERENCE.md)
- Check out the [tutorial](../apps/tutorial/README.md)

## Getting Help

If you encounter issues:

1. Check this troubleshooting section
2. Search [existing issues](https://github.com/Jameshunter1/VectorDBMS/issues)
3. Open a [new issue](https://github.com/Jameshunter1/VectorDBMS/issues/new) with:
   - Your OS and compiler version
   - CMake version
   - Complete error message
   - Steps to reproduce
