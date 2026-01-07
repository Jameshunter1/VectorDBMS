# Contributing to LSM Vector Database Engine

Thank you for your interest in contributing! This document provides guidelines for contributing to the project.

## Table of Contents
- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Workflow](#development-workflow)
- [Coding Standards](#coding-standards)
- [Testing Requirements](#testing-requirements)
- [Pull Request Process](#pull-request-process)
- [Vector Database Guidelines](#vector-database-guidelines)

## Code of Conduct

We are committed to providing a welcoming and inclusive environment. Be respectful and professional in all interactions.

## Getting Started

### Prerequisites
- C++20 compatible compiler (MSVC 2022, GCC 13+, Clang 17+)
- CMake 3.24 or later
- Git

### Clone and Build
```powershell
# Clone repository
git clone https://github.com/yourusername/LSMDatabaseEngine.git
cd LSMDatabaseEngine/src

# Build
cmake --preset windows-msvc-debug  # or appropriate preset
cmake --build --preset debug

# Run tests
ctest --preset debug --output-on-failure
```

## Development Workflow

1. **Fork the repository** and create your branch from `main`
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Make your changes** following our coding standards

3. **Write tests** for new functionality

4. **Run the test suite** and ensure all tests pass
   ```powershell
   ctest --preset debug --output-on-failure
   ```

5. **Commit your changes** with clear, descriptive messages
   ```bash
   git commit -m "feat: add vector batch delete operation"
   ```

6. **Push to your fork** and submit a pull request

## Coding Standards

### C++ Style Guide
- **C++20 Standard**: Use modern C++20 features
- **Naming Conventions**:
  - Classes/Structs: `PascalCase` (e.g., `HNSWIndex`, `VectorSearch`)
  - Functions/Methods: `PascalCase` (e.g., `PutVector`, `SearchSimilar`)
  - Variables: `snake_case` (e.g., `vector_dimension`, `entry_point_`)
  - Private members: `snake_case_` with trailing underscore (e.g., `nodes_`, `mutex_`)
  - Constants: `kPascalCase` (e.g., `kDefaultDimension`)

### Code Organization
- **Headers**: `include/core_engine/` - Public API
- **Implementation**: `lib/` - Internal implementation
- **Namespace**: All code in `namespace core_engine` or sub-namespaces

### Example
```cpp
namespace core_engine {
namespace vector {

class HNSWIndex {
 public:
  Status Insert(const std::string& key, const Vector& vec);
  
 private:
  std::vector<Node> nodes_;
  mutable std::shared_mutex mutex_;
  static constexpr std::size_t kDefaultM = 16;
};

}  // namespace vector
}  // namespace core_engine
```

### Error Handling
- Use `Status` return types for operations that can fail
- **Never throw exceptions** in core engine code
- Document error conditions in comments

### Threading
- Use `std::shared_mutex` for read-heavy data structures
- Document thread-safety guarantees
- Prefer lock-free algorithms where appropriate

## Testing Requirements

### Test Coverage
All new features must include:
1. **Unit tests** - Test individual components in isolation
2. **Integration tests** - Test component interactions
3. **Performance tests** - Benchmark critical paths

### Test Structure
```cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("VectorIndex::Insert adds vector to index", "[vector]") {
  HNSWIndex index(/*params*/);
  Vector vec({1.0f, 2.0f, 3.0f});
  
  Status s = index.Insert("key1", vec);
  
  REQUIRE(s.ok());
  REQUIRE(index.GetStats().num_vectors == 1);
}
```

### Running Tests
```powershell
# All tests
ctest --preset debug --output-on-failure

# Specific test suite
.\build\windows-msvc-debug\Debug\test_vector_engine.exe

# With verbose output
.\build\windows-msvc-debug\Debug\test_vector_engine.exe --success
```

## Pull Request Process

1. **Update documentation** for any API changes
2. **Add tests** for new functionality
3. **Run all tests** locally before submitting
4. **Update CHANGELOG.md** with your changes
5. **Fill out the PR template** completely
6. **Request review** from maintainers

### PR Title Format
Use conventional commits:
- `feat:` - New features
- `fix:` - Bug fixes
- `perf:` - Performance improvements
- `docs:` - Documentation changes
- `test:` - Test additions/changes
- `refactor:` - Code refactoring

Example: `feat: add cosine similarity metric for vector search`

## Vector Database Guidelines

### Vector Operations
When working on vector database features:

1. **Dimension Validation**: Always validate vector dimensions match configuration
2. **Distance Metrics**: Support all standard metrics (cosine, euclidean, dot product, manhattan)
3. **Thread Safety**: Vector index must support concurrent reads
4. **Performance**: Target O(log N) search complexity with HNSW
5. **Memory Efficiency**: Consider memory usage for large-scale deployments

### HNSW Index Development
- Maintain graph connectivity during node deletion
- Tune `M` (connections) vs memory tradeoff
- Optimize `ef_construction` and `ef_search` defaults
- Document complexity and memory characteristics

### Example Vector Feature PR
```cpp
// Add new distance metric support
Status HNSWIndex::Insert(const std::string& key, const Vector& vec) {
  // 1. Validate input
  if (vec.dimension() != params_.dimension) {
    return Status::InvalidArgument("Dimension mismatch");
  }
  
  // 2. Acquire appropriate lock
  std::unique_lock lock(mutex_);
  
  // 3. Update index
  InsertNode(key, vec);
  
  // 4. Return status
  return Status::OK();
}
```

## Performance Considerations

### Benchmarking New Features
```cpp
// benchmarks/bench_vector.cpp
BENCHMARK("VectorInsert/1000vectors") {
  HNSWIndex index(params);
  for (int i = 0; i < 1000; ++i) {
    index.Insert(GenerateKey(i), GenerateRandomVector(128));
  }
};
```

### Target Performance
- Vector insertion: < 1ms per vector (128D)
- Similarity search (k=10): < 10ms (100K vectors)
- Batch operations: 10x faster than individual operations

## Questions or Need Help?

- **Documentation**: See [README.md](../README.md) and [VECTOR_DATABASE_GUIDE.md](../VECTOR_DATABASE_GUIDE.md)
- **Issues**: Open an issue with the `question` label
- **Discussions**: Use GitHub Discussions for design questions

Thank you for contributing! 🚀
