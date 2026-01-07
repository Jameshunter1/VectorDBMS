# What We Built: A Working Database Engine

## Overview

We built a **database engine** in C++20 that stores and retrieves data permanently on disk. Think of it like a smart dictionary that never forgets — even after power loss or crashes.

It uses an **LSM-tree** architecture (Log-Structured Merge-tree), the same proven design behind production databases like RocksDB, Cassandra, and LevelDB.

## What It Does

This is a **key-value store** — you give it a key and a value, and it saves them to disk. Simple as a dictionary, but permanent and fast.

**Example:**
```
PUT "username" "alice"
GET "username"  → returns "alice"
DELETE "username"  → removes the key (writes tombstone)
GET "username"  → returns NOT FOUND
```

## Key Features

### 1. **Write-Ahead Log** (Crash Protection)
- Every write is **immediately saved** to `wal.log` before anything else
- If the program crashes mid-operation, it replays this log on restart to recover your data
- Think of it as a backup diary that records every change

### 2. **MemTable** (Fast Recent Data)
- Recent writes stay in memory for **instant access**
- Keeps data sorted by key (alphabetically) for efficient searching
- When it reaches 4 MB, it gets saved to disk automatically

### 3. **SSTables** (Permanent Storage)
- **SSTable** = Sorted String Table (a file with sorted key-value pairs)
- Created when MemTable fills up and needs to be saved to disk
- **Immutable** = once written, never modified (only read or deleted)
- Multiple SSTables can exist, each containing different data

### 4. **Bloom Filters** (Smart Search Optimization)
- A **space-efficient checker** that answers: "Is this key definitely NOT in this file?"
- Saves time by skipping files that don't contain the key you're looking for
- Uses only ~1.25 bytes per key but prevents 90%+ of unnecessary file reads
- May rarely say "maybe" when the answer is "no" (false positive ~1%)

### 5. **Compaction** (Automatic Cleanup)
- When 4+ SSTables exist, they're **merged into one larger file**
- Removes duplicate keys (keeps the newest value)
- Frees up disk space and speeds up reads
- Runs automatically in the background

### 6. **Recovery** (Survives Restarts)
- On startup, replays the Write-Ahead Log to rebuild the in-memory MemTable
- All data returns, even after crashes or power loss

### 7. **Delete Operations** (Tombstones)
- **Tombstone** = Special marker that indicates a key has been deleted
- Deletes are instant — just write a tombstone to the WAL and MemTable
- Key becomes immediately invisible to reads (Get returns NOT FOUND)
- Physical removal happens later during compaction (when tombstone and older values are merged)

## How Users Interact With It

### 1. **Command-Line Tool (dbcli)**
```powershell
# Store data
dbcli.exe ./my_database put "key1" "value1"

# Retrieve data
dbcli.exe ./my_database get "key1"
```

### 2. **Web Frontend (dbweb)**
- Open `http://localhost:8080` in your browser
- Type keys and values in a simple form
- Click "Put" to store, "Get" to retrieve
- Watch the database files grow on disk

### 3. **As a Library**
```cpp
Engine engine;
engine.Open("./db");
engine.Put("hello", "world");
auto value = engine.Get("hello");  // returns "world"
```

## Why This Is a Real Database

This isn't a toy project. It has production-grade features:

✅ **Durability** — Data survives crashes, power loss, and restarts  
✅ **Real Files** — Creates actual `wal.log` and `sstable_*.sst` files on disk  
✅ **Automatic Optimization** — Compaction and flushing happen automatically  
✅ **Performance Optimizations** — Bloom filters reduce unnecessary disk reads by 90%+  
✅ **Tested** — Comprehensive test suite covering all features  
✅ **Production Architecture** — Same LSM design as RocksDB, Cassandra, LevelDB

## Visible Artifacts

When you use the engine, you'll see these files:

```
my_database/
  ├── wal.log           ← Every write is logged here
  └── sstable_0.sst     ← Flushed data (sorted, immutable)
```

After compaction:
```
my_database/
  ├── wal.log           ← Still logging new writes
  └── sstable_5.sst     ← Compacted from multiple older files
```

## Performance Characteristics

- **Writes:** Very fast — append to log file + insert into memory (microseconds)
- **Reads (recent data):** Very fast — served directly from MemTable (nanoseconds)
- **Reads (old data):** Fast — Bloom filters skip 90%+ of SSTables, binary search within files
- **Space:** Automatically freed by merging old SSTables during compaction
- **Durability:** Guaranteed — every write hits disk via Write-Ahead Log

## Performance Statistics (Visible in Web UI)

- **MemTable Size** — how much data is in memory
- **SSTable Count** — number of files on disk
- **WAL Size** — size of the recovery log
- **Bloom Filter Checks** — how many times we consulted Bloom filters
- **Bloom Filter Hits** — how many file reads we avoided (higher = faster)
- **Bloom Filter Hit Rate** — percentage of successful optimizations

## What's Next (Future Milestones)

1. **Multi-Level Compaction** — organize SSTables into levels (Level 0, 1, 2...) for better performance
2. **Range Queries** — get all keys between "apple" and "banana"
3. **SQL Layer** — parse and execute SQL queries on top of the key-value store
4. **Snapshots** — consistent point-in-time views of the database
5. **Secondary Indexes** — fast lookups by non-primary key fields

## Glossary

**Key-Value Store** — Database that maps keys to values (like a dictionary: "username" → "alice")

**LSM-Tree** — Log-Structured Merge-tree. Architecture that writes everything to a log first, then organizes data later

**Write-Ahead Log (WAL)** — Log file (`wal.log`) that records every write before it happens. Used for crash recovery

**MemTable** — In-memory sorted map holding recent writes. Flushed to disk when it reaches 4 MB

**SSTable** — Sorted String Table. Immutable file on disk containing sorted key-value pairs (e.g., `sstable_0.sst`)

**Immutable** — Cannot be changed. Once an SSTable is written, it's never modified (only read or deleted)

**Compaction** — Process of merging multiple SSTables into one, removing duplicates and freeing space

**Bloom Filter** — Space-efficient probabilistic data structure that tests whether a key is definitely NOT in a file

**False Positive** — When Bloom filter says "maybe it's there" but the key actually isn't (happens ~1% of the time)

**Binary Search** — Efficient search algorithm for sorted data (like searching a phone book by opening to the middle)

**Tombstone** — Special marker that indicates a key has been deleted (future feature)

## Why This Matters

You now have a **working storage engine** that could be embedded in other applications:
- Add persistence to any C++ program
- Foundation for building a full database (add SQL, transactions, indexes)
- Educational: understand how real databases work under the hood

This is the hard part of building a database — the storage layer that keeps data safe and fast.
