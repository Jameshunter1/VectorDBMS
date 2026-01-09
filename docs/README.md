# VectorDBMS Documentation

Welcome to the VectorDBMS documentation! This directory contains all project documentation organized by topic.

## 📚 Documentation Index

### Getting Started

- **[Quick Start Guide](../README.md#quick-start)** - Get up and running in 5 minutes
- **[Build & Run Commands](BUILD_AND_RUN_COMMANDS.md)** - Step-by-step build instructions for all platforms
- **[Command Reference](COMMAND_REFERENCE.md)** - Complete reference of all CLI commands, build commands, and workflows
- **[GitHub MCP Setup](MCP_SETUP.md)** - Integration guide for AI assistants

### Complete Documentation

- **[Complete Documentation](DOCUMENTATION.md)** - Comprehensive guide covering:
  - Building from source (Windows, Linux, macOS)
  - Docker deployment
  - API reference (HTTP & C++)
  - Performance tuning
  - Security features
  - Contributing guidelines

### Project Policies

- **[Contributing Guidelines](CONTRIBUTING.md)** - How to contribute to the project
- **[Code of Conduct](CODE_OF_CONDUCT.md)** - Community standards and expectations

### Additional Resources

- **[Cleanup Summary](CLEANUP_SUMMARY.md)** - Project cleanup and refactoring notes
- **[Scripts Documentation](../scripts/README.md)** - PowerShell scripts for demos and utilities
- **[Source Code Documentation](../src/README.md)** - Source code organization
- **[Python SDK](../python-sdk/README.md)** - Python client library

## 🚀 Quick Links

### For Developers

```powershell
# Windows - Build and test
cmake --preset=windows-vs2022-x64-debug
cmake --build build/windows-vs2022-x64-debug
cd build/windows-vs2022-x64-debug && ctest -C Debug --output-on-failure
```

```bash
# Linux/macOS - Build and test
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

### For Users

- **HTTP API**: See [API Reference](DOCUMENTATION.md#api-reference)
- **Docker**: See [Docker Deployment](DOCUMENTATION.md#docker-deployment)
- **Performance**: See [Performance Guide](DOCUMENTATION.md#performance)

## 📖 Documentation Structure

```
docs/
├── README.md                    # This file - documentation index
├── BUILD_AND_RUN_COMMANDS.md   # Build instructions
├── COMMAND_REFERENCE.md         # Complete command reference
├── DOCUMENTATION.md             # Complete documentation
├── MCP_SETUP.md                 # GitHub MCP setup guide
├── CONTRIBUTING.md              # Contribution guidelines
├── CODE_OF_CONDUCT.md          # Code of conduct
└── CLEANUP_SUMMARY.md          # Cleanup notes
```

## 🔗 External Links

- **[GitHub Repository](https://github.com/Jameshunter1/VectorDBMS)**
- **[Issue Tracker](https://github.com/Jameshunter1/VectorDBMS/issues)**
- **[Discussions](https://github.com/Jameshunter1/VectorDBMS/discussions)**

## 💡 Need Help?

1. Check the [Command Reference](COMMAND_REFERENCE.md) for specific commands
2. Read the [Complete Documentation](DOCUMENTATION.md) for in-depth guides
3. Browse [GitHub Issues](https://github.com/Jameshunter1/VectorDBMS/issues) for known problems
4. Ask questions in [GitHub Discussions](https://github.com/Jameshunter1/VectorDBMS/discussions)

---

*Last updated: January 2026*
