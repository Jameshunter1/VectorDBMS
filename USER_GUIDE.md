# LSM Database Engine - Quick Start Guide

A simple, fast database that works like SQLite but uses modern Log-Structured Merge-tree technology.

## What is This?

Think of it as a **super-fast key-value store** that:
- ✅ Saves your data permanently (like a file, but better)
- ✅ Never loses data (even if your computer crashes)
- ✅ Gets faster as you add more data
- ✅ Works on Windows, Linux, and Mac

## For Beginners

### What Can I Store?

- **Keys**: Short names like `"user_123"`, `"config"`, `"session:abc"`
- **Values**: Any text like `"John Doe"`, `{"name":"Alice"}`, or long documents

### Where to Use It?

- Desktop apps that need to save settings
- Games that need to save player data
- Web apps that need a fast cache
- Any app that needs simple, fast storage

## Getting Started

### 1. Web Interface (Easiest Way)

**Windows:**
```powershell
cd src
.\build\windows-vs2022-x64-debug\Debug\dbweb.exe
```

**Linux/Mac:**
```bash
cd src
./build/linux/dbweb
```

Then open your browser to: **http://localhost:8080**

You'll see a beautiful interface where you can:
- 📝 **PUT**: Save data (key + value)
- 🔍 **GET**: Retrieve data (by key)
- 🗑️ **DELETE**: Remove data
- 📊 **Stats**: See how your database is doing

### 2. Command Line Interface

**Windows:**
```powershell
.\build\windows-vs2022-x64-debug\Debug\dbcli.exe
```

**Linux/Mac:**
```bash
./build/linux/dbcli
```

Type commands like:
```
put user_1 Alice
get user_1
delete user_1
```

### 3. In Your Own Code

**Simple Example:**
```cpp
#include <core_engine/engine.hpp>

Engine db;
db.Open("./my_data");  // Creates folder to store data

// Save data
db.Put("user_1", "Alice");
db.Put("user_2", "Bob");

// Retrieve data
auto value = db.Get("user_1");  // Returns "Alice"

// Delete data
db.Delete("user_1");
```

That's it! Three simple operations: Put, Get, Delete.

## Understanding How It Works

### The Basics

1. **Write-Ahead Log (WAL)**: 
   - When you save data, it's written here first (super fast!)
   - This is your safety net - if the program crashes, your data is still here

2. **MemTable** (In Memory):
   - Recent data lives here temporarily
   - Lightning fast reads and writes
   - When it fills up (4 MB), it gets saved to disk

3. **SSTables** (On Disk):
   - Permanent storage files
   - Your data lives here forever
   - Organized in levels for fast searching

### What Happens When You Save Data?

```
You: db.Put("user_1", "Alice")
  ↓
[1] Written to WAL (crash-safe!) ← Takes 1 millisecond
  ↓
[2] Added to MemTable (in RAM) ← Takes 0.001 milliseconds
  ↓
[3] MemTable fills up? Flush to SSTable on disk
  ↓
[4] Too many SSTables? Compact them automatically
```

### What Happens When You Read Data?

```
You: db.Get("user_1")
  ↓
[1] Check MemTable first (super fast!)
  ↓
[2] Not there? Check recent SSTables
  ↓
[3] Use Bloom Filter to skip files that definitely don't have it
  ↓
[4] Return value!
```

## Real-World Examples

### Example 1: User Settings

```cpp
Engine db;
db.Open("./user_settings");

// Save user preferences
db.Put("theme", "dark");
db.Put("language", "english");
db.Put("notifications", "enabled");

// Load on startup
auto theme = db.Get("theme");  // "dark"
```

### Example 2: Session Management

```cpp
Engine db;
db.Open("./sessions");

// User logs in
db.Put("session:abc123", R"({"user":"alice","expires":"2026-01-06"})");

// Check session later
auto session = db.Get("session:abc123");
if (session.has_value()) {
  // Session exists!
}

// User logs out
db.Delete("session:abc123");
```

### Example 3: Game High Scores

```cpp
Engine db;
db.Open("./game_data");

// Save scores
db.Put("player:alice:score", "9500");
db.Put("player:bob:score", "12000");
db.Put("player:carol:score", "8000");

// Get all scores
auto entries = db.GetAllEntries();
// Sort and display top 10
```

## Performance Tips

### Fast Operations
✅ **Single Puts**: 10,000-50,000 per second  
✅ **Single Gets**: 50,000-200,000 per second  
✅ **Small values**: Faster than large ones

### What Slows Things Down?
❌ **Very large values** (> 1 MB each)  
❌ **Too many deletes** (creates "tombstones")  
❌ **Syncing every write** (safe but slower)

### How to Speed Things Up

**1. Batch your writes:**
```cpp
// SLOW: One at a time
for (int i = 0; i < 1000; i++) {
  db.Put("key_" + std::to_string(i), "value");
}

// FAST: Batch them (future feature)
db.BatchPut(many_keys_and_values);
```

**2. Use async mode for non-critical data:**
```cpp
auto config = DatabaseConfig::Development("./cache");
config.wal_sync_mode = DatabaseConfig::WalSyncMode::kNone;
db.Open(config);  // 10x faster, but less safe
```

**3. Tune memory settings:**
```cpp
auto config = DatabaseConfig::Production("./db");
config.memtable_flush_threshold_bytes = 64 * 1024 * 1024;  // 64 MB
config.block_cache_size_bytes = 512 * 1024 * 1024;         // 512 MB
db.Open(config);
```

## Monitoring Your Database

### Check Statistics

```cpp
auto stats = db.GetStats();
std::cout << "Entries: " << stats.memtable_entry_count << "\n";
std::cout << "MemTable: " << stats.memtable_size_bytes << " bytes\n";
std::cout << "SSTables: " << stats.sstable_count << " files\n";
std::cout << "Total reads: " << stats.total_gets << "\n";
std::cout << "Total writes: " << stats.total_puts << "\n";
```

### Healthy Database Signs

✅ MemTable size stays under 4 MB  
✅ SSTable count stays under 20 files  
✅ WAL size resets after flushes  
✅ Bloom filter hit rate > 80%

### Warning Signs

⚠️ MemTable stuck at max size → Flushes not happening  
⚠️ Too many SSTables (> 50) → Compaction falling behind  
⚠️ WAL keeps growing (> 100 MB) → System can't keep up  

## Troubleshooting

### "Database won't open"

**Problem**: Folder doesn't exist or no permissions  
**Solution**:
```cpp
std::filesystem::create_directories("./my_db");
db.Open("./my_db");
```

### "Writes are slow"

**Problem**: Syncing to disk after every write (safe mode)  
**Solution**: Use async mode if you can tolerate some data loss:
```cpp
auto config = DatabaseConfig::Development("./db");
config.wal_sync_mode = DatabaseConfig::WalSyncMode::kNone;
db.Open(config);
```

### "Can't find my data"

**Problem**: Key was deleted or misspelled  
**Solution**: Check the key name carefully (case-sensitive!)
```cpp
db.Put("User", "Alice");
db.Get("user");  // NOT FOUND! (capital U)
db.Get("User");  // Found!
```

### "Database is huge"

**Problem**: Lots of deleted keys (tombstones) taking space  
**Solution**: Compaction will eventually clean them up. Or restart:
```cpp
db.Close();
db.Open("./db");  // Replays WAL, cleans tombstones
```

## What's Next?

### Advanced Features (Coming Soon)

- **Range Queries**: Get all keys between "a" and "z"
- **Snapshots**: Save the database state at a point in time
- **Batch Operations**: Insert thousands of keys at once
- **Compression**: Automatically compress large values
- **Replication**: Mirror data to other servers

### Learn More

- **Production Deployment**: See [PRODUCTION_DEPLOYMENT.md](PRODUCTION_DEPLOYMENT.md)
- **Example Code**: Check [apps/examples/](apps/examples/)
- **Architecture**: Read the source code comments in [lib/lsm/](lib/lsm/)

## Need Help?

- 💬 **Questions**: Open a GitHub Discussion
- 🐛 **Bugs**: Open a GitHub Issue
- 📖 **Documentation**: Check the `/docs` folder

---

**Happy coding! 🚀**
