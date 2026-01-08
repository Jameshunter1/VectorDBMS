# Vectis Web Frontend Demo
param(
    [string]$BaseUrl = "http://localhost:8080"
)

Write-Host "`n=== VECTIS WEB FRONTEND DEMO ===" -ForegroundColor Cyan
Write-Host "Web UI: $BaseUrl`n" -ForegroundColor Green

# 1. PUT Operations
Write-Host "[1] STORING DATA" -ForegroundColor Yellow
$testData = @(
    @{key="user:1001"; value='{"name":"Alice","age":28}'},
    @{key="user:1002"; value='{"name":"Bob","age":35}'},
    @{key="config:theme"; value="dark"},
    @{key="session:abc"; value='{"user":1001}'}
)

foreach ($item in $testData) {
    $url = "${BaseUrl}/api/put?key=$($item.key)&value=$($item.value)"
    Invoke-RestMethod -Uri $url -Method Post | Out-Null
    Write-Host "  [OK] PUT $($item.key)" -ForegroundColor Green
}

# 2. GET Operations
Write-Host "`n[2] RETRIEVING DATA" -ForegroundColor Yellow
$result = Invoke-RestMethod -Uri "${BaseUrl}/api/get?key=user:1001"
Write-Host "  GET user:1001 => $($result.value)" -ForegroundColor Cyan

# 3. Batch Operations
Write-Host "`n[3] BATCH OPERATIONS" -ForegroundColor Yellow
Write-Host "  Creating 50 test entries..." -ForegroundColor Cyan
1..50 | ForEach-Object {
    $key = "test_$_"
    $value = "value_$_"
    Invoke-RestMethod -Uri "${BaseUrl}/api/put?key=$key&value=$value" -Method Post | Out-Null
}
Write-Host "  [OK] Created 50 entries" -ForegroundColor Green

# 4. Statistics
Write-Host "`n[4] DATABASE STATISTICS" -ForegroundColor Yellow
$stats = Invoke-RestMethod -Uri "${BaseUrl}/api/stats"
Write-Host "  Total Entries: $($stats.total_entries)" -ForegroundColor Cyan
Write-Host "  Page Count: $($stats.page_count)" -ForegroundColor Cyan
$hitRate = [math]::Round($stats.buffer_hit_rate * 100, 2)
Write-Host "  Buffer Hit Rate: ${hitRate}%" -ForegroundColor Cyan

# 5. Browse Entries
Write-Host "`n[5] BROWSE ENTRIES (First 5)" -ForegroundColor Yellow
$browse = Invoke-RestMethod -Uri "${BaseUrl}/api/list?offset=0&limit=5"
foreach ($entry in $browse.entries) {
    Write-Host "  $($entry.key) = $($entry.value)" -ForegroundColor Gray
}

# 6. Delete Operation
Write-Host "`n[6] DELETE OPERATION" -ForegroundColor Yellow
Invoke-RestMethod -Uri "${BaseUrl}/api/delete?key=test_50" -Method Delete | Out-Null
Write-Host "  [OK] Deleted test_50" -ForegroundColor Green

# Final stats
Write-Host "`n[FINAL] DATABASE STATE" -ForegroundColor Yellow
$final = Invoke-RestMethod -Uri "${BaseUrl}/api/stats"
Write-Host "  Total Entries: $($final.total_entries)" -ForegroundColor Cyan
Write-Host "  Uptime: $([math]::Round($final.uptime_seconds, 1))s" -ForegroundColor Cyan

# UI Feature Guide
Write-Host "`n=== WEB UI FEATURES ===" -ForegroundColor Cyan
Write-Host "
Open $BaseUrl in your browser to explore:

TABS:
  1. Operations   - PUT/GET/DELETE with forms
  2. Batch Ops    - Generate bulk data
  3. Browse       - Paginated table view
  4. Range Scan   - Query key ranges
  5. Vector       - Similarity search (if enabled)
  6. Metrics      - Prometheus metrics
  7. Health       - Component health checks
  8. Files        - Database file explorer

FEATURES:
  - Real-time stats in header (auto-refresh 5s)
  - JSON syntax highlighting
  - Search/filter in browse tab
  - Pagination (50 entries/page)
  - Visual success/error feedback
" -ForegroundColor White

Write-Host "=== DEMO COMPLETE ===" -ForegroundColor Cyan
Write-Host "Open browser: $BaseUrl`n" -ForegroundColor Green
