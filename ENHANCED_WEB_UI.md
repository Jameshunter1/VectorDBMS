# Enhanced Web UI Guide

## What's New

The web interface has been completely redesigned with powerful features for testing the LSM-tree database engine.

## New Features

### 1. **Real-Time Database Statistics** 📊
- **MemTable Size**: Shows how much memory is currently used by the in-memory table
- **MemTable Entries**: Number of key-value pairs currently in memory
- **SSTable Files**: Number of persistent sorted table files on disk  
- **WAL Size**: Size of the write-ahead log
- **Auto-Refresh**: Statistics update automatically every 5 seconds and after each operation

### 2. **Bulk Insert Operations** ⚡
- Insert hundreds or thousands of keys at once
- Configure custom key prefix (e.g., "user", "product", "session")
- Watch MemTable flushes happen in real-time
- Observe automatic compaction when SSTable count reaches threshold
- Perfect for testing LSM behavior under load

### 3. **Batch Put Interface** 📝
- Insert multiple key=value pairs at once
- Simple format: one pair per line
- Example:
  ```
  username=alice
  email=alice@example.com  
  age=30
  city=New York
  ```
- Great for setting up test data quickly

### 4. **Enhanced Output Console** 📋
- Color-coded success/error messages
- Timestamps on every operation
- Auto-scrolling to show latest results
- Clear button to reset console

### 5. **Modern UI Design** 🎨
- Clean, professional gradient design
- Responsive layout that works on all screen sizes
- Visual stat cards with clear labels
- Intuitive button colors (blue=store, green=retrieve, etc.)
- Helpful hints explaining what each operation does

## Testing LSM Behavior

### Test 1: Watch a MemTable Flush
1. Click "🔄 Refresh Stats" to see current state
2. Set bulk prefix to "test"
3. Set count to 1000
4. Click "⚡ Bulk Insert"
5. Watch the MemTable size grow in real-time
6. When it hits 4MB, it will flush to an SSTable
7. You'll see MemTable entries drop to 0 and SSTable count increase by 1

### Test 2: Trigger Compaction
1. Perform multiple bulk inserts (e.g., 4 batches of 1000 keys each)
2. Each batch will create a new SSTable (if large enough)
3. When 4 or more SSTables exist, compaction automatically runs
4. Watch SSTable count reduce as files merge together

### Test 3: Data Persistence  
1. Insert some data using batch put
2. Stop the server (close terminal)
3. Restart the server
4. Try to retrieve the data - it's still there!
5. Check WAL size - it shows accumulated write log

## Understanding the Statistics

### MemTable Size
- **What it means**: Bytes of memory used by the in-memory buffer
- **Threshold**: Flushes to disk at 4MB (4,194,304 bytes)
- **Watch for**: Rapid growth during bulk inserts, then sudden drop to ~0

### MemTable Entries
- **What it means**: Number of unique keys currently in memory
- **Note**: Updates to existing keys don't increase this count
- **Watch for**: Drops to 0 after a flush

### SSTable Files
- **What it means**: Number of immutable sorted table files on disk
- **Threshold**: Compacts when 4+ files exist
- **Watch for**: Increases after flushes, decreases after compaction

### WAL Size
- **What it means**: Total size of the write-ahead log  
- **Purpose**: Enables recovery after crashes
- **Note**: Grows with every Put operation

## Educational Comments

The code is now heavily commented for C++ learners:

- **Header files**: Explains what `#include` does
- **Data types**: Describes `std::string`, `std::mutex`, `std::optional`
- **Operators**: Explains ternary operators, dereference (`*`), etc.
- **Lambdas**: Shows how `[&](...)` captures variables
- **HTTP concepts**: Explains status codes (200, 400, 404, 500)
- **Concurrency**: Describes mutex locking and why it's needed

## API Endpoints

The web UI uses these HTTP endpoints:

- `GET /` - Serves the HTML interface
- `GET /api/stats` - Returns database statistics as JSON
- `POST /api/put?key=X&value=Y` - Stores a key-value pair
- `GET /api/get?key=X` - Retrieves a value by key

## Behind the Scenes

When you click "Put":
1. JavaScript sends HTTP POST to `/api/put`
2. C++ server acquires mutex lock
3. Calls `engine.Put(key, value)`
4. Engine writes to WAL for durability
5. Engine stores in MemTable
6. If MemTable >= 4MB, triggers flush to SSTable
7. If SSTables >= 4, triggers compaction
8. Server returns "OK" response
9. JavaScript refreshes statistics
10. You see updated numbers!

## Tips for Learning

1. **Start Small**: Try single Put/Get operations first
2. **Observe Flushes**: Use bulk insert with 1000 keys to see a flush
3. **Trigger Compaction**: Do 4-5 bulk inserts to see compaction
4. **Check Persistence**: Restart the server and verify data survives
5. **Read the Code**: Open [main.cpp](../src/apps/dbweb/main.cpp) and follow the educational comments

## Next Steps

Want to dive deeper?

- [ ] Add a "Delete" operation (requires tombstone support)
- [ ] Show list of all keys in the database
- [ ] Add query-by-prefix support
- [ ] Visualize LSM tree structure
- [ ] Add Bloom filter statistics
- [ ] Show compaction history/timeline

Happy testing! 🚀
