# LSM Database Engine - Quick Reference Card

## 🚀 Getting Started (30 seconds)

```powershell
# 1. Start the server
.\demo_web_simple.ps1

# 2. Open browser
http://127.0.0.1:8080

# 3. Start using!
```

---

## ⚡ Common Operations

### Insert Data
```
Tab: Operations
1. Enter key: user_123
2. Enter value: {"name":"Alice"}
3. Click [PUT]
```

### Find Entry
```
Tab: Browse Data
1. Type in search: user_
2. Results filter instantly
3. Click [View] to edit
```

### Generate Test Data
```
Tab: Operations → Bulk Operations
1. Prefix: test
2. Count: 100
3. Click [Generate]
```

### Export Backup
```
Tab: Browse Data
1. Click [Export JSON]
2. Saves: lsmdb-export-2026-01-05.json
```

---

## 📊 Monitoring

### Check Performance
```
Tab: Statistics
- Watch MemTable size grow
- Monitor read/write times
- Check bloom filter hit rate
```

### View Files
```
Tab: Files
1. Click [Refresh]
2. See level_0/, level_1/, level_2/
3. Check SSTable sizes
```

### Watch Activity
```
Tab: Console
- Real-time operation logs
- Color-coded messages
- Auto-scrolls to latest
```

---

## 🔍 Searching & Filtering

### Search by Key Prefix
```
Search box: user_
Shows: user_1, user_2, user_123, etc.
```

### Search by Value Content
```
Search box: alice
Shows: All entries with "alice" in value
```

### Sort Results
```
Dropdown: Sort A → Z (or Z → A)
Table updates instantly
```

---

## 📋 Pagination

### Navigate Pages
```
[← Prev]  Page 1 of 10  [Next →]
Click arrows to move between pages
```

### Change Page Size
```
Dropdown: [25 per page ▼]
Options: 10, 25, 50, 100
```

### See Total Count
```
Header: "Database Entries (1,234 total)"
Always shows full count
```

---

## 💾 Bulk Operations

### Batch Insert (Manual)
```
Tab: Operations → Batch Insert
Format (one per line):
  user_1=Alice
  user_2=Bob
  admin=Charlie
Click [Batch Insert]
```

### Generate Test Data (Auto)
```
Tab: Operations → Generate Test Data
Prefix: test
Count: 500
Click [Generate]
Creates: test_0, test_1, ... test_499
```

---

## 🗑️ Deleting Data

### Delete Single Entry
```
Tab: Browse Data
1. Find entry in table
2. Click [Delete]
3. Confirm deletion
```

### Clear All Entries
```
Tab: Browse Data
1. Click [Clear All]
2. Confirm: "Delete all entries?"
3. All data removed
⚠️ Cannot be undone!
```

---

## 📁 File Management

### View Database Files
```
Tab: Files
Shows:
  📁 level_0/
    📄 data_00001.sst (4.0 MB)
  📁 level_1/
  📄 wal_00001.log (256 KB)
  📄 MANIFEST (4 KB)
```

### Refresh File List
```
Tab: Files → [Refresh]
Updates after flush/compaction
```

---

## 📈 Understanding Statistics

### Key Metrics

| Metric | What it Means | Good Value |
|--------|---------------|------------|
| **MemTable Size** | RAM usage | < 4 MB (auto-flushes at 4MB) |
| **Entries** | Items in memory | Varies |
| **SSTables** | Files on disk | Fewer = better (after compaction) |
| **WAL Size** | Unflushed writes | < 1 MB |
| **Bloom Hit Rate** | Filter accuracy | 70-90% |
| **Avg Read Time** | GET speed | < 100 µs |
| **Avg Write Time** | PUT speed | < 50 µs |

### Progress Bar
- **Green fill**: MemTable usage toward 4 MB
- **100% = Flush triggered**

---

## 🎨 Color Coding

### Buttons
- 🟣 **Purple** = Primary action (PUT, Generate)
- 🟢 **Green** = Safe action (GET, View, Export)
- 🔴 **Red** = Delete action (DELETE, Clear)
- ⚪ **Gray** = Utility (Refresh)

### Console Messages
- 🟢 **Green** = Success (✓ PUT "key")
- 🔴 **Red** = Error (✗ Key not found)
- 🔵 **Blue** = Info (Batch inserting...)

---

## ⌨️ Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Tab` | Navigate fields |
| `Enter` | Submit form |
| `Ctrl+F` | Focus search box |
| `Escape` | Clear input |

---

## 🔧 Troubleshooting

### No entries showing
```
✓ Click [Refresh] in Browse tab
✓ Check search box is empty
✓ Verify Console shows successful PUTs
```

### Stats show 0
```
✓ Switch to another tab and back
✓ Wait 5 seconds (auto-refresh)
✓ Verify server is running
```

### Can't find entry
```
✓ Check spelling in search box
✓ Try searching value instead of key
✓ Clear search and browse all pages
```

### Export downloads empty
```
✓ Refresh Browse tab first
✓ Verify entries visible in table
✓ Check browser downloads folder
```

---

## 🎯 Use Cases

### Development Testing
```
1. Generate 1000 test entries
2. Monitor statistics during insert
3. Check file creation in Files tab
4. Export data for comparison
```

### Data Migration
```
1. Use Batch Insert with CSV data
2. Verify entries in Browse tab
3. Export as JSON backup
4. Check file sizes in Files tab
```

### Performance Analysis
```
1. Clear database
2. Generate 5000 entries
3. Monitor read/write times in Stats
4. Check bloom filter effectiveness
```

### Debugging
```
1. Insert test data
2. Watch Console for errors
3. Check Files tab for SSTables
4. Verify stats make sense
```

---

## 📊 Data Limits

| Item | Limit | Notes |
|------|-------|-------|
| **Key Size** | ~64 KB | Practical limit |
| **Value Size** | ~1 MB | Larger = slower |
| **MemTable** | 4 MB | Auto-flushes |
| **Total Entries** | Millions | Limited by disk space |
| **Browser Display** | 10,000 | Smooth with pagination |

---

## 🌐 API Reference (Advanced)

### Endpoints

```
GET  /                    → Web interface HTML
GET  /api/stats           → JSON statistics
GET  /api/entries         → JSON all entries
GET  /api/files           → JSON file listing
POST /api/put             → Insert/update entry
GET  /api/get?key=X       → Retrieve entry
POST /api/delete          → Remove entry
```

### Example: Command Line
```powershell
# Insert
Invoke-RestMethod -Uri "http://127.0.0.1:8080/api/put" `
  -Method Post -Body "key=test&value=data"

# Get
Invoke-RestMethod -Uri "http://127.0.0.1:8080/api/get?key=test"

# Stats
Invoke-RestMethod -Uri "http://127.0.0.1:8080/api/stats"
```

---

## 💡 Pro Tips

### 1. Test Large Datasets
```
Generate 10,000 entries to test pagination
Change page size to see performance
```

### 2. Monitor Compaction
```
1. Generate 5000+ entries
2. Watch Stats: MemTable fills → Flushes
3. Check Files: level_0/ appears
4. Later: Files move to level_1/
```

### 3. Search Efficiently
```
Use key prefixes for faster results
Example: "user_" instead of just "u"
```

### 4. Export Regularly
```
Click Export JSON after major changes
Keep backups in safe location
```

### 5. Use Console for Debugging
```
Leave Console tab open in another window
See real-time operation feedback
```

---

## 📱 Mobile Access

The interface works on mobile browsers:
- URL: http://127.0.0.1:8080
- All tabs accessible
- Touch-friendly buttons
- Tables scroll horizontally
- Forms stack vertically

---

## 🎓 Learning Path

### Beginner (15 minutes)
1. Insert 10 entries manually
2. Use Browse tab to view them
3. Try search and pagination
4. Check Statistics tab

### Intermediate (30 minutes)
1. Generate 500 test entries
2. Practice searching and filtering
3. Export data as JSON
4. View files in Files tab

### Advanced (1 hour)
1. Generate 5000+ entries
2. Monitor MemTable flush
3. Watch SSTable creation
4. Analyze bloom filter hit rate
5. Test different page sizes
6. Practice bulk operations

---

## 🆘 Help Resources

- **[FEATURES_ENHANCED_UI.md](FEATURES_ENHANCED_UI.md)** - Complete feature guide
- **[MILESTONE_ENHANCED_UI.md](MILESTONE_ENHANCED_UI.md)** - Technical details
- **[VISUAL_TOUR.md](VISUAL_TOUR.md)** - UI walkthrough
- **[USER_GUIDE.md](USER_GUIDE.md)** - General database guide

---

## 🎉 Quick Win

**Get up and running in 1 minute:**

```powershell
# 1. Start
.\demo_web_simple.ps1

# 2. Wait for browser to open

# 3. In Operations tab:
#    - Prefix: demo
#    - Count: 50
#    - Click [Generate]

# 4. Switch to Browse tab
#    - See your 50 entries!
#    - Try searching: demo_1
#    - Click [Export JSON]

# Done! You've used:
#  ✓ Bulk operations
#  ✓ Browse/search
#  ✓ Export
```

---

**Need more help?** Check the [FEATURES_ENHANCED_UI.md](FEATURES_ENHANCED_UI.md) for detailed explanations!

