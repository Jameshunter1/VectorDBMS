# Quick Reference Card

## Starting the Database

```powershell
# Build (first time or after code changes)
cd "c:\Users\James\SystemProjects\New folder\src"
cmake --build build/windows-vs2022-x64-debug

# Start web interface
./build/windows-vs2022-x64-debug/Debug/dbweb.exe ./_web_demo

# Start CLI interface
./build/windows-vs2022-x64-debug/Debug/dbcli.exe ./_cli_demo
```

## Web Interface (http://127.0.0.1:8080/)

### Statistics Panel
| Stat | Meaning | Watch For |
|------|---------|-----------|
| **MemTable Size** | Bytes in memory | Drops to ~0 after 4MB (flush) |
| **MemTable Entries** | Keys in memory | Drops to 0 after flush |
| **SSTable Files** | Files on disk | Decreases during compaction |
| **WAL Size** | Write log size | Always increases |

### Operations

#### Single Key
```
Key:   username
Value: alice
→ Put → Get
```

#### Bulk Insert
```
Prefix: user
Count:  1000
→ Creates user_0 through user_999
```

#### Batch Put
```
username=alice
email=alice@example.com
age=30
```

## LSM-Tree Behavior

### When Does Flush Happen?
- **Trigger**: MemTable reaches 4 MB
- **Effect**: MemTable → SSTable file
- **Result**: MemTable clears, SSTable count +1

### When Does Compaction Happen?
- **Trigger**: 4 or more SSTable files exist
- **Effect**: Multiple SSTables → 1 merged SSTable
- **Result**: SSTable count decreases, duplicates removed

### Read Path (Get Operation)
```
1. Check MemTable (fastest)
   ↓ not found
2. Check SSTable #1
   ↓ not found
3. Check SSTable #2
   ↓ not found
... continue until found or exhausted
```

## Key Thresholds

| Item | Threshold | What Happens |
|------|-----------|--------------|
| MemTable | **4 MB** | Flush to SSTable |
| SSTables | **4 files** | Compact to 1 file |
| WAL | No limit | Grows forever |

## Common Tasks

### Test Flush
1. Bulk insert 1000 keys
2. Watch MemTable Size → 4MB → 0B
3. Watch SSTable Count → +1

### Test Compaction
1. Bulk insert 4 times (different prefixes)
2. Watch SSTable Count → 4 → 1-2
3. All data still retrievable

### Test Persistence
1. Insert data
2. Stop server (Ctrl+C)
3. Restart server
4. Retrieve data → still there!

## File Structure
```
_web_demo/
├── wal.log           ← Write-ahead log
├── sstable_0.dat     ← First SSTable
├── sstable_1.dat     ← Second SSTable
└── ...
```

## Keyboard Shortcuts (Browser)

| Action | Shortcut |
|--------|----------|
| Refresh page | F5 |
| Hard refresh | Ctrl+F5 |
| Clear console | (Click 🗑️ button) |

## HTTP Status Codes

| Code | Meaning | Example |
|------|---------|---------|
| **200** | OK | Successful Put/Get |
| **400** | Bad Request | Missing key parameter |
| **404** | Not Found | Key doesn't exist |
| **500** | Server Error | Internal failure |

## CLI Commands

```
# Start CLI
./build/windows-vs2022-x64-debug/Debug/dbcli.exe ./_cli_demo

# Commands
put <key> <value>   # Store
get <key>           # Retrieve
help                # Show help
quit                # Exit
```

## Useful Docs

| Doc | Purpose |
|-----|---------|
| [WHAT_WE_BUILT.md](WHAT_WE_BUILT.md) | Simple explanation |
| [ENHANCED_WEB_UI.md](ENHANCED_WEB_UI.md) | UI features guide |
| [WEB_UI_TESTING_GUIDE.md](WEB_UI_TESTING_GUIDE.md) | Testing scenarios |
| [CPP_CONCEPTS.md](CPP_CONCEPTS.md) | C++ tutorial |
| [UPDATE_SUMMARY.md](UPDATE_SUMMARY.md) | Recent changes |

## Troubleshooting

### Server Won't Start
```
Error: Address already in use
→ Solution: Kill existing dbweb.exe process
```

### Statistics Show "—"
```
→ Solution: Click "🔄 Refresh Stats" or reload page
```

### Build Fails
```
→ Solution: Delete build folder and reconfigure:
rm -r build/windows-vs2022-x64-debug
cmake --preset windows-vs2022-x64-debug
cmake --build build/windows-vs2022-x64-debug
```

### Can't Find Key After Flush
```
→ This shouldn't happen - data persists!
→ Check WAL and SSTable files exist in db directory
→ Check Output Console for error messages
```

## Advanced: Inspecting Files

### View WAL Size
```powershell
Get-Item ./_web_demo/wal.log | Select-Object Length
```

### List SSTables
```powershell
Get-ChildItem ./_web_demo/sstable_*.dat
```

### Total Database Size
```powershell
(Get-ChildItem ./_web_demo -Recurse | Measure-Object -Property Length -Sum).Sum
```

## Performance Expectations

| Operation | Speed | Notes |
|-----------|-------|-------|
| Single Put | ~1 ms | Writes to WAL + MemTable |
| Single Get (MemTable) | ~0.1 ms | In-memory lookup |
| Single Get (SSTable) | ~1 ms | Disk read + binary search |
| Bulk Insert (1000) | ~1-5 s | Depends on flush/compaction |
| Flush | ~100 ms | MemTable → SSTable |
| Compaction | ~500 ms | Multiple SSTables → 1 |

## Fun Experiments

1. **Stress Test**: Insert 10,000 keys and watch compaction
2. **Size Test**: Insert very long values (100KB+) and watch MemTable size
3. **Conflict Test**: Update same key repeatedly (only size changes once)
4. **Alphabet Test**: Insert a-z keys and verify alphabetical order
5. **Recovery Test**: Insert 1000 keys, crash server (Ctrl+C), restart, verify all keys exist

---

**🎯 Pro Tip**: Keep this tab open while testing - it's your cheat sheet!

**📚 Next**: Read [WEB_UI_TESTING_GUIDE.md](WEB_UI_TESTING_GUIDE.md) for detailed scenarios

**💡 Learning**: Study [CPP_CONCEPTS.md](CPP_CONCEPTS.md) while exploring code
