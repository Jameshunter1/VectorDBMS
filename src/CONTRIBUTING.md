# Contributing to VectorDBMS

Thank you for your interest in contributing to VectorDBMS! We welcome contributions from the community.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [How to Contribute](#how-to-contribute)
- [Pull Request Process](#pull-request-process)
- [Coding Standards](#coding-standards)
- [Testing Guidelines](#testing-guidelines)
- [Reporting Bugs](#reporting-bugs)
- [Suggesting Enhancements](#suggesting-enhancements)

## Code of Conduct

This project adheres to a Code of Conduct that all contributors are expected to follow. Please read [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) before contributing.

## Getting Started

1. Fork the repository on GitHub
2. Clone your fork locally:
   ```bash
   git clone https://github.com/YOUR-USERNAME/VectorDBMS.git
   cd VectorDBMS
   ```
3. Add the upstream repository:
   ```bash
   git remote add upstream https://github.com/Jameshunter1/VectorDBMS.git
   ```

## Development Setup

### Prerequisites

- CMake 3.20 or higher
- C++20 compatible compiler (MSVC 2022, GCC 11+, or Clang 13+)
- Git

### Building the Project

#### Windows (Visual Studio 2022)

```powershell
cmake --preset windows-vs2022-x64-debug
cmake --build --preset windows-vs2022-x64-debug
```

#### Linux/macOS

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Running Tests

```bash
cd build
ctest --output-on-failure
```

## How to Contribute

### Types of Contributions

We welcome various types of contributions:

- **Bug fixes**: Fix issues reported in the issue tracker
- **Features**: Implement new features or enhancements
- **Documentation**: Improve or add documentation
- **Tests**: Add or improve test coverage
- **Performance**: Optimize existing code
- **Refactoring**: Improve code quality and maintainability

### Before You Start

1. Check the [issue tracker](https://github.com/Jameshunter1/VectorDBMS/issues) for existing issues
2. For major changes, open an issue first to discuss your proposal
3. Make sure tests pass before submitting a PR

## Pull Request Process

1. **Create a branch**: Use a descriptive branch name
   ```bash
   git checkout -b feature/your-feature-name
   # or
   git checkout -b fix/issue-description
   ```

2. **Make your changes**:
   - Write clear, concise commit messages
   - Follow the coding standards (see below)
   - Add tests for new functionality
   - Update documentation as needed

3. **Keep your branch updated**:
   ```bash
   git fetch upstream
   git rebase upstream/main
   ```

4. **Run tests locally**:
   ```bash
   cmake --build build --target test
   ```

5. **Push to your fork**:
   ```bash
   git push origin feature/your-feature-name
   ```

6. **Open a Pull Request**:
   - Use a clear, descriptive title
   - Reference any related issues (e.g., "Fixes #123")
   - Describe what changes you made and why
   - Include screenshots for UI changes

7. **Address review feedback**:
   - Respond to comments and requested changes
   - Push additional commits to your branch
   - Mark conversations as resolved when addressed

### PR Requirements

- ✅ All tests pass
- ✅ Code follows project style guidelines
- ✅ New code has test coverage
- ✅ Documentation is updated
- ✅ No merge conflicts with main branch
- ✅ PR description clearly explains changes

## Coding Standards

### C++ Style Guidelines

- **Modern C++**: Use C++20 features where appropriate
- **Naming Conventions**:
  - Classes/Structs: `PascalCase` (e.g., `StorageEngine`)
  - Functions/Methods: `camelCase` (e.g., `getValue()`)
  - Variables: `snake_case` (e.g., `page_size`)
  - Constants: `UPPER_SNAKE_CASE` (e.g., `MAX_BUFFER_SIZE`)
  - Private members: prefix with `m_` (e.g., `m_buffer`)

- **Formatting**:
  - Indentation: 4 spaces (no tabs)
  - Line length: Max 100 characters
  - Braces: Opening brace on same line
  
  ```cpp
  if (condition) {
      // code here
  }
  ```

- **Best Practices**:
  - Use RAII for resource management
  - Prefer `std::unique_ptr` and `std::shared_ptr` over raw pointers
  - Use `const` wherever possible
  - Avoid `using namespace std;` in headers
  - Document complex algorithms and design decisions

### Code Organization

- Header files go in `include/core_engine/`
- Implementation files go in `lib/`
- Tests go in `tests/`
- Keep files focused and modular

### Documentation

- Use Doxygen-style comments for public APIs:
  ```cpp
  /**
   * @brief Retrieves a value from the database
   * @param key The key to lookup
   * @return The value associated with the key
   * @throws KeyNotFoundException if key doesn't exist
   */
  std::string getValue(const std::string& key);
  ```

## Testing Guidelines

### Writing Tests

- Use descriptive test names that explain what is being tested
- Follow the Arrange-Act-Assert pattern
- Test both success and failure cases
- Include edge cases and boundary conditions

### Test Structure

```cpp
TEST(ComponentTest, DescriptiveTestName) {
    // Arrange
    Component component;
    
    // Act
    auto result = component.doSomething();
    
    // Assert
    EXPECT_EQ(expected, result);
}
```

### Running Specific Tests

```bash
cd build
ctest -R test_name_pattern
```

## Reporting Bugs

When reporting bugs, please include:

1. **Clear title**: Brief description of the issue
2. **Environment**: OS, compiler version, CMake version
3. **Steps to reproduce**: Minimal code example
4. **Expected behavior**: What should happen
5. **Actual behavior**: What actually happens
6. **Logs/Error messages**: Full error output
7. **Screenshots**: If applicable

Use the [bug report template](../../issues/new?template=bug_report.md) when creating an issue.

## Suggesting Enhancements

For feature requests or enhancements:

1. **Search existing issues**: Check if it's already proposed
2. **Provide context**: Explain the use case
3. **Describe the solution**: What you'd like to see implemented
4. **Consider alternatives**: Mention other approaches you've considered
5. **Additional context**: Any relevant examples or mockups

Use the [feature request template](../../issues/new?template=feature_request.md) when creating an issue.

## Performance Considerations

When contributing performance-sensitive code:

- Run benchmarks before and after changes
- Document performance implications
- Consider memory usage and allocation patterns
- Profile your changes if possible

## Questions?

- Open a [discussion](../../discussions) for general questions
- Join our community channels (if available)
- Check existing documentation and API references

## Recognition

Contributors will be recognized in:
- The project's README
- Release notes for significant contributions
- GitHub contributor graphs

Thank you for helping make VectorDBMS better! 🚀
