# Changelog

All notable changes to VectorDBMS will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- GitHub repository with professional development structure
- Comprehensive CI/CD pipelines
- Code coverage tracking
- Security scanning workflows
- Issue and PR templates

## [1.2.0] - 2026-01-06

### Added
- **Enhanced Web UI**: Complete redesign with modern interface
  - Pagination support (10/25/50/100 items per page)
  - Real-time search across keys and values
  - Sorting capabilities (A→Z, Z→A)
  - File browser for database files (SSTables, WAL, MANIFEST)
  - Export functionality (download all data as JSON)
  - Individual entry view/edit modal
  - Live operation logging with timestamps
- **File Management**: Browse and inspect database file system
- **Statistics Dashboard**: 10 real-time metrics with visual progress bars
- **Bulk Operations**: Import/export functionality

### Improved
- Web interface performance with pagination
- Memory efficiency for large datasets (handles 10,000+ entries)
- User experience with modern, responsive design
- Database browse functionality

### Fixed
- Performance issues with large datasets in web UI
- Memory usage during bulk operations

## [1.1.0] - Previous Release

### Added
- Basic web interface (`dbweb`)
- Command-line interface (`dbcli`)
- Core LSM-tree storage engine
- Write-Ahead Log (WAL) for durability
- SSTable management with compaction
- MANIFEST file for version tracking

### Features
- PUT/GET/DELETE operations
- Memtable with sorted structure
- SSTable generation and persistence
- Background compaction
- Crash recovery

## [1.0.0] - Initial Release

### Added
- Initial page-based database engine
- Key-value storage with LSM-tree architecture
- Basic persistence layer
- Memory management
- Transaction support
- Security features

---

## Version History Summary

### Version 1.2.x - Enhanced UI & UX
Focus: Production-ready web interface with enterprise features

### Version 1.1.x - Core Functionality
Focus: Stable database engine with basic interfaces

### Version 1.0.x - Foundation
Focus: Core architecture and basic operations

---

## Upgrade Guide

### From 1.1.x to 1.2.0

No breaking changes. The database format is fully compatible.

**Web Interface Updates**:
- The web UI now uses pagination - adjust your expectations for initial page load
- Export functionality added - you can now download all data as JSON
- File browser added - inspect database internals

**Performance**:
- Better handling of large datasets
- Improved memory efficiency

### From 1.0.x to 1.1.x

Database format is compatible, but it's recommended to:
1. Backup your data
2. Test with the new version
3. Monitor performance

---

## Notes

- **[Unreleased]**: Changes in `main` branch not yet released
- **[Version]**: Released versions with dates
- Categories: Added, Changed, Deprecated, Removed, Fixed, Security

For detailed commit history, see [GitHub Commits](https://github.com/Jameshunter1/VectorDBMS/commits/).
