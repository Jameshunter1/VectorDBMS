# Testing the Enhanced Web UI - Step by Step

## Getting Started

### 1. Start the Web Server
```powershell
cd "c:\Users\James\SystemProjects\New folder\src"
./build/windows-vs2022-x64-debug/Debug/dbweb.exe ./_web_demo
```

You should see:
```
[INFO] Opening database at: ./_web_demo
[INFO] dbweb running
[INFO] Open http://127.0.0.1:8080/
[INFO] DB dir: ./_web_demo
[INFO] (Operations write to WAL, trigger flushes/compaction automatically)
```

### 2. Open Your Browser
Navigate to: **http://127.0.0.1:8080/**

You'll see a modern interface with:
- 📊 Database Statistics section (top)
- 🔑 Single Key Operations
- 📦 Bulk Operations
- 📝 Batch Put section
- 📋 Output Console (bottom)

---

## Test Scenario 1: Basic Operations

### Step 1: View Initial Statistics
- Click "🔄 Refresh Stats"
- You should see:
  - MemTable Size: **0 B** (empty)
  - MemTable Entries: **0**
  - SSTable Files: **0** (none created yet)
  - WAL Size: **0 B** (no writes yet)

### Step 2: Insert a Single Key
- In the "Single Key Operations" section:
  - Key: `username`
  - Value: `alice`
- Click "💾 Put (Store)"
- Watch the Output Console show:
  ```
  [timestamp] ✓ PUT "username" = "alice" → OK (written to WAL + MemTable, may trigger flush/compaction)
  ```
- Statistics automatically update:
  - MemTable Size: **~25 B** (small increase)
  - MemTable Entries: **1**
  - WAL Size: **~25 B**

### Step 3: Retrieve the Value
- Key: `username` (same as before)
- Click "🔍 Get (Retrieve)"
- Output Console shows:
  ```
  [timestamp] ✓ GET "username" → "alice"
  ```

### Step 4: Try a Missing Key
- Key: `nonexistent`
- Click "🔍 Get (Retrieve)"
- Output Console shows:
  ```
  [timestamp] ✗ GET "nonexistent" → NOT FOUND
  ```

---

## Test Scenario 2: Batch Insert Multiple Keys

### Step 1: Prepare Batch Data
In the "Batch Put (Multiple Keys)" textarea, enter:
```
username=alice
email=alice@example.com
age=30
city=New York
country=USA
role=admin
status=active
```

### Step 2: Execute Batch Insert
- Click "💾 Batch Put"
- Watch the Output Console show each operation:
  ```
  [timestamp] Processing 7 key=value pairs...
  [timestamp]   ✓ PUT "username"
  [timestamp]   ✓ PUT "email"
  [timestamp]   ✓ PUT "age"
  [timestamp]   ✓ PUT "city"
  [timestamp]   ✓ PUT "country"
  [timestamp]   ✓ PUT "role"
  [timestamp]   ✓ PUT "status"
  [timestamp] Batch complete: 7/7 successful
  ```

### Step 3: Verify Statistics
- MemTable Size: **~200 B** (increased)
- MemTable Entries: **7**
- SSTable Files: **0** (still too small to flush)

---

## Test Scenario 3: Trigger a MemTable Flush

### Goal: Fill the MemTable to 4MB and watch it flush to an SSTable

### Step 1: Configure Bulk Insert
- Key prefix: `user`
- Count: `1000`

### Step 2: Execute Bulk Insert
- Click "⚡ Bulk Insert"
- Watch the Output Console:
  ```
  [timestamp] Starting bulk insert: 1000 keys with prefix "user"...
  [timestamp]   Progress: 50/1000 keys inserted...
  [timestamp]   Progress: 100/1000 keys inserted...
  ...
  [timestamp]   Progress: 1000/1000 keys inserted...
  [timestamp] ✓ Bulk insert complete: 1000/1000 keys in X.XXs
  ```

### Step 3: Observe Statistics Changes
Depending on your key/value sizes, you might need multiple runs. When MemTable reaches 4MB:

**Before flush:**
- MemTable Size: **~3.8 MB**
- MemTable Entries: **1000+**
- SSTable Files: **0**

**After flush (automatic):**
- MemTable Size: **~0 B** (cleared!)
- MemTable Entries: **0** (cleared!)
- SSTable Files: **1** (new file created!)
- WAL Size: **~3.8 MB** (accumulated writes)

### Step 4: Verify Data Persistence
- Try retrieving one of the keys: `user_0`
- Click "🔍 Get (Retrieve)"
- Output shows: `✓ GET "user_0" → "value_0_..."`
- The data is now on disk in an SSTable!

---

## Test Scenario 4: Trigger Compaction

### Goal: Create 4+ SSTables to trigger automatic compaction

### Step 1: Create Multiple SSTables
Repeat bulk inserts with different prefixes (each should create a new SSTable):

1. Prefix: `test1`, Count: `1000` → Creates SSTable #1
2. Prefix: `test2`, Count: `1000` → Creates SSTable #2
3. Prefix: `test3`, Count: `1000` → Creates SSTable #3
4. Prefix: `test4`, Count: `1000` → Creates SSTable #4

After each bulk insert, watch the SSTable count increase.

### Step 2: Watch Compaction Trigger
When SSTable count reaches **4**, compaction automatically runs!

**Before compaction:**
- SSTable Files: **4**

**After compaction:**
- SSTable Files: **1** or **2** (merged!)

### Step 3: Verify Data Integrity
All your data is still accessible:
- Try retrieving `test1_0`
- Try retrieving `test4_999`
- All keys should still return their values!

---

## Test Scenario 5: Data Persistence Across Restarts

### Step 1: Insert Some Data
```
config_server=production
config_port=8080
config_debug=false
```

### Step 2: Stop the Server
- Close the terminal or press `Ctrl+C`

### Step 3: Restart the Server
```powershell
./build/windows-vs2022-x64-debug/Debug/dbweb.exe ./_web_demo
```

### Step 4: Verify Data Survived
- Refresh the browser page (http://127.0.0.1:8080/)
- Try retrieving `config_server`
- Output shows: `✓ GET "config_server" → "production"`
- **Data persisted!** 🎉

This works because:
1. WAL logs every write
2. On startup, WAL is replayed into MemTable
3. SSTables are reloaded from disk

---

## Understanding the Statistics

### MemTable Size
- **Grows**: As you insert keys
- **Resets to ~0**: After flushing to SSTable
- **Threshold**: 4 MB (4,194,304 bytes)

### MemTable Entries
- **Grows**: With each unique key
- **Doesn't grow**: When updating existing keys
- **Resets to 0**: After flush

### SSTable Files
- **Grows**: Each time MemTable flushes
- **Shrinks**: When compaction merges files
- **Trigger**: Compaction runs at 4+ files

### WAL Size
- **Always grows**: Every Put operation appends
- **Large**: Accumulates all writes
- **Purpose**: Recovery after crashes

---

## Tips & Tricks

### Clear the Output Console
- Click "🗑️ Clear" button in Output Console section

### Auto-Refresh Statistics
- Statistics refresh automatically every 5 seconds
- Also refresh after every operation

### Experiment with Different Data
Try inserting:
- Long keys: `very_long_key_name_with_many_characters`
- Long values: `This is a very long value with lots of text...`
- Special characters: `key=value=with=equals`

### Watch File System Changes
While testing, open your file explorer to `_web_demo/` folder:
- See `wal.log` grow as you insert data
- See `sstable_*.dat` files appear after flushes
- Watch file sizes change during compaction

---

## Common Issues & Solutions

### "Stats error" in Console
**Cause**: Server not running or wrong port

**Solution**: 
- Check terminal shows "dbweb running"
- Verify URL is http://127.0.0.1:8080/

### Statistics Show Wrong Values
**Cause**: Old browser cache

**Solution**:
- Hard refresh: `Ctrl+F5` (Windows) or `Cmd+Shift+R` (Mac)
- Or click "🔄 Refresh Stats" button

### SSTable Count Doesn't Decrease After Many Inserts
**Cause**: Compaction only triggers at 4+ files

**Solution**:
- Make sure you have at least 4 SSTables
- Each SSTable needs to be created from a full MemTable (4MB)

---

## Next Steps

Congratulations! You've tested all the major features. Now you can:

1. **Read the Code**: Open [main.cpp](src/apps/dbweb/main.cpp) and follow the comments
2. **Understand LSM**: Read [WHAT_WE_BUILT.md](WHAT_WE_BUILT.md)
3. **Learn C++**: Study [CPP_CONCEPTS.md](CPP_CONCEPTS.md)
4. **Add Features**: Try implementing new operations (delete, scan, etc.)

Happy exploring! 🚀
