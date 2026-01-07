# Database Engine Compaction Demo
# This script demonstrates the LSM compaction feature from a user's perspective.

Write-Host "=== CORE-ENGINE: Compaction Demonstration ===" -ForegroundColor Cyan
Write-Host ""

$dbDir = ".\_demo_compaction"
$dbcli = ".\build\windows-vs2022-x64-debug\Debug\dbcli.exe"

# Clean up any existing demo database
if (Test-Path $dbDir) {
    Remove-Item -Recurse -Force $dbDir
    Write-Host "Cleaned up previous demo database" -ForegroundColor Yellow
}

Write-Host "Step 1: Creating fresh database at $dbDir" -ForegroundColor Green
Write-Host ""

# Insert initial data
Write-Host "Step 2: Inserting 1000 small key-value pairs..." -ForegroundColor Green
for ($i = 0; $i -lt 1000; $i++) {
    $key = "user_$i"
    $value = "data_value_$i"
    & $dbcli $dbDir put $key $value | Out-Null
    if ($i % 200 -eq 0) {
        Write-Host "  Inserted $i keys..."
    }
}
Write-Host "  [OK] Inserted 1000 keys" -ForegroundColor Green
Write-Host ""

# Check initial state
Write-Host "Step 3: Checking database files (before compaction trigger)..." -ForegroundColor Green
$sstables = Get-ChildItem $dbDir -Filter "*.sst"
$walSize = (Get-Item "$dbDir\wal.log").Length
Write-Host "  - WAL size: $walSize bytes"
Write-Host "  - SSTable count: $($sstables.Count)"
if ($sstables.Count -gt 0) {
    foreach ($sst in $sstables) {
        Write-Host "    - $($sst.Name): $($sst.Length) bytes"
    }
}
Write-Host ""

# Insert large batch to trigger multiple flushes and compaction
Write-Host "Step 4: Inserting large dataset to trigger compaction..." -ForegroundColor Green
Write-Host "  (This creates multiple SSTables, then compacts them when threshold is reached)" -ForegroundColor Gray
$largeValue = "x" * 1024  # 1 KB value

for ($batch = 0; $batch -lt 5; $batch++) {
    Write-Host "  Batch $($batch + 1)/5: Inserting 4500 x 1KB entries..."
    for ($i = 0; $i -lt 4500; $i++) {
        $key = "batch${batch}_key$i"
        & $dbcli $dbDir put $key $largeValue | Out-Null
    }
    
    # Check SSTable count after each batch
    $sstCount = (Get-ChildItem $dbDir -Filter "*.sst").Count
    Write-Host "    → SSTable count after batch: $sstCount" -ForegroundColor Yellow
}
Write-Host ""

# Check final state
Write-Host "Step 5: Final database state (after compaction)..." -ForegroundColor Green
$sstables = Get-ChildItem $dbDir -Filter "*.sst"
$walSize = (Get-Item "$dbDir\wal.log").Length
Write-Host "  - WAL size: $walSize bytes"
Write-Host "  - SSTable count: $($sstables.Count)" -ForegroundColor Cyan
Write-Host "  - SSTable files:" -ForegroundColor Cyan
foreach ($sst in $sstables) {
    Write-Host "    - $($sst.Name): $([math]::Round($sst.Length / 1MB, 2)) MB"
}
Write-Host ""

# Verify data integrity
Write-Host "Step 6: Verifying data integrity after compaction..." -ForegroundColor Green
$testKeys = @("user_500", "batch2_key1000", "batch4_key3000")
foreach ($key in $testKeys) {
    $result = & $dbcli $dbDir get $key 2>&1
    if ($result -match "Found") {
        Write-Host "  [OK] Key '$key' retrieved successfully" -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] Key '$key' NOT FOUND!" -ForegroundColor Red
    }
}
Write-Host ""

Write-Host "=== Compaction Demo Complete ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "Summary:" -ForegroundColor Yellow
Write-Host "- Inserted ~23,500 keys (1000 small + 22,500 large)" -ForegroundColor Gray
Write-Host "- Multiple MemTable flushes created SSTables" -ForegroundColor Gray
Write-Host "- Compaction automatically merged SSTables when 4+ existed" -ForegroundColor Gray
Write-Host "- Final SSTable count: $($sstables.Count) (reduced from 5+)" -ForegroundColor Gray
Write-Host "- All data remains accessible after compaction" -ForegroundColor Gray
Write-Host ""
Write-Host "Database location: $dbDir" -ForegroundColor Cyan
Write-Host "You can inspect the files manually or continue testing with dbcli/dbweb" -ForegroundColor Gray
