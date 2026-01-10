# Contributing to Vectis Database Engine

Thank you for your interest in Vectis! This is currently a personal learning project to demonstrate professional database engineering practices.

## Project Status

This project is in **active development** (Year 1 Q1 complete). While external contributions are not being accepted at this time, you are welcome to:

- 🍴 Fork the repository for educational purposes
- 📖 Study the architecture and implementation
- 💡 Provide feedback via GitHub Issues
- ⭐ Star the repository if you find it useful

## Development Workflow

The project follows a structured branching model:

- **`master`** - Stable releases (quarterly milestones)
- **`develop`** - Active development branch
- **`feature/*`** - Feature branches for specific components
- **`hotfix/*`** - Critical bug fixes

## Code Standards

### C++ Guidelines
- **C++20** standard required
- **Clang-format** enforced (LLVM style, 100-char line limit)
- **Every line commented** explaining WHY, not just WHAT
- **Explicit memory management** - no hidden allocations
- **Return `Status`** instead of throwing exceptions in core paths

### Commit Message Format
```
<type>: <subject>

<body>

<footer>
```

Types: `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `chore`

Example:
```
feat: Implement BufferPoolManager with LRU-K eviction

- Add PageTable for page_id → frame mapping
- Implement pin/unpin semantics with guard objects
- Add background flush thread for dirty pages
- Thread-safe with fine-grained latching

Year 1 Q2 milestone complete.
```


All new features must include:

Tests must pass before merging:
```powershell
ctest --preset vs-debug --output-on-failure
```

## Questions?

Open a GitHub Issue with:


**Note**: Once Year 1 milestones are complete (Q4: WAL implementation), the project may open for community contributions. Check back for updates!
## Monitoring & Milestone Requirements

All new features that affect metrics or monitoring should:
- Expose new metrics for Prometheus if relevant
- Update Grafana dashboards if metrics change
- Document milestone tracking in DOCUMENTATION.md
