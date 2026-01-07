# Production Deployment Guide

**Quick Summary**: This database works like SQLite but is faster for write-heavy workloads. You can run it in three modes: Embedded (single folder), Production (separate disks), or Development (fast but less safe).

## File Organization

Your database creates these folders:

```
my_database/
├── wal.log          # Safety log (writes go here first)
├── MANIFEST         # Index of all data files
├── level_0/         # Recent data (small files)
├── level_1/         # Compacted data (medium files)
└── level_2/         # Old data (large files)
```

**Why levels?** Newer data (level_0) is checked first. Older data (level_2) is bigger but checked less often.

## Three Ways to Deploy

### Mode 1: Embedded (Simple)

**Best for**: Desktop apps, games, small tools

**Setup** (just one line):
```cpp
Engine db;
db.Open("./my_data");  // That's it!
```

Everything goes in one folder. Simple to backup - just copy the folder.

### Mode 2: Production (Fast Disk for Writes)

**Best for**: Web servers, APIs, high-traffic apps

**Setup** (separate fast disk for writes):
```cpp
#include <core_engine/common/config.hpp>

auto config = DatabaseConfig::Production("/var/lib/mydb");
config.wal_dir = "/mnt/fast-ssd/wal";     // Put logs on fast SSD
config.data_dir = "/mnt/big-hdd/data";    // Put data on big HDD

Engine db;
db.Open(config);
```

**Why?** Writes go to fast disk (SSD), reads come from cheap disk (HDD). Best of both worlds!

### Mode 3: Development (Maximum Speed)

**Best for**: Testing, local development

**Setup** (no safety guarantees):
```cpp
auto config = DatabaseConfig::Development("./dev_db");
config.wal_sync_mode = DatabaseConfig::WalSyncMode::kNone;  // Skip safety!
db.Open(config);
```

**Warning**: If your computer crashes, you might lose recent data. Only use for testing!

## Where to Put Your Files

### Linux
```bash
# Production server
/var/lib/mydb/              # Main folder
  wal/wal.log              # On fast disk
  data/                     # On big disk

# Give permissions
sudo chown -R myapp:myapp /var/lib/mydb
```

### Windows
```powershell
# Production server
C:\ProgramData\MyApp\database\
  wal\wal.log              # On C:\ (SSD)
  data\                     # On D:\ (HDD)

# Set permissions for your service account
```

### Mac
```bash
# Desktop app
~/Library/Application Support/MyApp/database/
```

## Making It Faster

### Memory Settings

**Small App** (limited RAM):
```cpp
config.memtable_flush_threshold_bytes = 4 * 1024 * 1024;   // 4 MB
config.block_cache_size_bytes = 64 * 1024 * 1024;          // 64 MB
```

**Big Server** (lots of RAM):
```cpp
config.memtable_flush_threshold_bytes = 64 * 1024 * 1024;  // 64 MB
config.block_cache_size_bytes = 512 * 1024 * 1024;         // 512 MB
```

### Safety vs Speed

**Maximum Safety** (banking, healthcare):
```cpp
config.wal_sync_mode = DatabaseConfig::WalSyncMode::kEveryWrite;
```

**Maximum Speed** (analytics, caching):
```cpp
config.wal_sync_mode = DatabaseConfig::WalSyncMode::kNone;
```

## Health Checks

### Quick Stats

```cpp
auto stats = db.GetStats();
std::cout << "Entries: " << stats.memtable_entry_count << "\n";
std::cout << "Memory: " << stats.memtable_size_bytes / 1024 << " KB\n";
std::cout << "Files: " << stats.sstable_count << "\n";
```

### Good Signs ✅

- MemTable under 4 MB
- Less than 20 SSTable files
- Bloom filter hit rate > 80%

### Bad Signs ⚠️

- MemTable stuck at max → Flushes failing
- More than 50 SSTables → Compaction falling behind
- WAL keeps growing → System overloaded

## Backups

### Simple Backup

```bash
# Stop the app
systemctl stop myapp

# Copy everything
cp -r /var/lib/mydb /backup/mydb-$(date +%Y%m%d)

# Restart
systemctl start myapp
```

### Restore

```bash
# Stop the app
systemctl stop myapp

# Replace files
rm -rf /var/lib/mydb
cp -r /backup/mydb-20260105 /var/lib/mydb

# Start (automatic recovery happens)
systemctl start myapp
```

## High Availability (Future)

- **Master-Replica**: WAL shipping for read replicas
- **Multi-Master**: Conflict-free replicated data types (CRDTs)
- **Cloud Storage**: S3-backed SSTables (SlateDB-style)

## Common Problems

### Slow Writes

**Problem**: Every write takes > 10ms  
**Fix**: Turn off fsync (if you can tolerate some data loss)
```cpp
config.wal_sync_mode = DatabaseConfig::WalSyncMode::kNone;
```

### Database Won't Start

**Problem**: "Cannot open database"  
**Fix**: Check folder permissions
```bash
ls -la /var/lib/mydb
chmod 755 /var/lib/mydb
```

## What's Next?

- 📚 **User Guide**: See [USER_GUIDE.md](USER_GUIDE.md) for basics
- 💻 **Example Code**: Check [apps/examples/production_deployment.cpp](src/apps/examples/production_deployment.cpp)
- 🐛 **Issues**: Open a GitHub issue if something doesn't work

---

**That's it!** Your database is ready for production. 🚀
