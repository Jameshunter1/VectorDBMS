#!/usr/bin/env pwsh
# Vectis Web Frontend Demonstration
# Demonstrates all features of the web interface via API calls

$baseUrl = "http://localhost:8080"

Write-Host "`n==================================================================="-ForegroundColor Cyan
Write-Host "  VECTIS DATABASE - WEB FRONTEND DEMONSTRATION" -ForegroundColor Cyan
Write-Host "===================================================================" -ForegroundColor Cyan

Write-Host "`n+ Web UI available at: " -NoNewline -ForegroundColor Green
Write-Host "$baseUrl" -ForegroundColor White

# 1. Store Data (PUT)
Write-Host "`n[1] STORING DATA" -ForegroundColor Yellow
Write-Host "------------------------------------------------------------" -ForegroundColor Gray
$data = @{
    "user:1001" = '{"name":"Alice","email":"alice@example.com","age":28}'
    "user:1002" = '{"name":"Bob","email":"bob@example.com","age":35}'
    "user:1003" = '{"name":"Charlie","email":"charlie@example.com","age":42}'
    "config:theme" = "dark"
    "config:language" = "en-US"
    "session:abc123" = '{"user_id":1001,"expires":"2026-01-08T00:00:00Z"}'
}

foreach ($key in $data.Keys) {
    $value = $data[$key]
    $escapedValue = [uri]::EscapeDataString($value)
    $url = "$baseUrl/api/put?key=$key" + "&value=$escapedValue"
    $response = Invoke-RestMethod -Uri $url -Method Post
    Write-Host "  + PUT $key" -ForegroundColor Green
}

# 2. Retrieve Data (GET)
Write-Host "`n[2] RETRIEVING DATA" -ForegroundColor Yellow
Write-Host "------------------------------------------------------------" -ForegroundColor Gray
$response = Invoke-RestMethod -Uri "$baseUrl/api/get?key=user:1001"
Write-Host "  GET user:1001 → " -NoNewline -ForegroundColor Cyan
Write-Host $response.value -ForegroundColor White

# 3. Batch Operations
Write-Host "`n[3] BATCH OPERATIONS" -ForegroundColor Yellow
Write-Host "------------------------------------------------------------" -ForegroundColor Gray
Write-Host "  Creating 100 test entries..." -ForegroundColor Cyan
for ($i = 1; $i -le 100; $i++) {
    $key = "test_$i"
    $value = "value_$i"
    $url = "$baseUrl/api/put?key=$key" + "&value=$value"
    Invoke-RestMethod -Uri $url -Method Post | Out-Null
}
Write-Host "  + Created 100 entries (test_1 to test_100)" -ForegroundColor Green

# Batch GET
$keys = @("test_1", "test_50", "test_100")
Write-Host "`n  Batch GET: " -NoNewline -ForegroundColor Cyan
$body = @{ keys = $keys } | ConvertTo-Json
$batchResponse = Invoke-RestMethod -Uri "$baseUrl/api/batch/get" -Method Post -Body $body -ContentType "application/json"
Write-Host "$($batchResponse.results.Count) values retrieved" -ForegroundColor White

# 4. Range Scan
Write-Host "`n[4] RANGE SCAN" -ForegroundColor Yellow
Write-Host "------------------------------------------------------------" -ForegroundColor Gray
$url = "$baseUrl/api/range?start_key=test_1" + "&end_key=test_20" + "&limit=10"
$rangeResponse = Invoke-RestMethod -Uri $url
Write-Host "  RANGE [test_1, test_20] limit 10 → " -NoNewline -ForegroundColor Cyan
Write-Host "$($rangeResponse.entries.Count) entries" -ForegroundColor White
foreach ($entry in $rangeResponse.entries) {
    Write-Host "    $($entry.key) = $($entry.value)" -ForegroundColor Gray
}

# 5. Vector Search (if enabled)
Write-Host "`n[5] VECTOR OPERATIONS (IF ENABLED)" -ForegroundColor Yellow
Write-Host "------------------------------------------------------------" -ForegroundColor Gray
$vector = @(1..128 | ForEach-Object { Get-Random -Minimum 0.0 -Maximum 1.0 })
$vectorBody = @{ key = "doc:vector1"; vector = $vector } | ConvertTo-Json -Compress

try {
    Invoke-RestMethod -Uri "$baseUrl/api/vector/add" -Method Post -Body $vectorBody -ContentType "application/json"
    Write-Host "  + Added vector 'doc:vector1' (128D)" -ForegroundColor Green
} catch {
    Write-Host "  ! Vector endpoint not available or vector operations disabled" -ForegroundColor Yellow
}

# 6. Database Statistics
Write-Host "`n[6] DATABASE STATISTICS" -ForegroundColor Yellow
Write-Host "------------------------------------------------------------" -ForegroundColor Gray
$stats = Invoke-RestMethod -Uri "$baseUrl/api/stats"
Write-Host "  Total Entries:    " -NoNewline -ForegroundColor Cyan
Write-Host $stats.total_entries -ForegroundColor White
Write-Host "  Buffer Hit Rate:  " -NoNewline -ForegroundColor Cyan
Write-Host "$([math]::Round($stats.buffer_hit_rate * 100, 2))%" -ForegroundColor White
Write-Host "  Page Count:       " -NoNewline -ForegroundColor Cyan
Write-Host $stats.page_count -ForegroundColor White

# 7. Prometheus Metrics
Write-Host "`n[7] PROMETHEUS METRICS" -ForegroundColor Yellow
Write-Host "------------------------------------------------------------" -ForegroundColor Gray
$metrics = Invoke-RestMethod -Uri "$baseUrl/metrics"
$metricLines = $metrics -split "`n" | Where-Object { $_ -match "^vectis_" -and $_ -notmatch "^#" } | Select-Object -First 5
foreach ($line in $metricLines) {
    Write-Host "  $line" -ForegroundColor Gray
}
Write-Host "  ... (50+ metrics available)" -ForegroundColor Gray

# 8. Delete Operations
Write-Host "`n[8] DELETE OPERATIONS" -ForegroundColor Yellow
Write-Host "------------------------------------------------------------" -ForegroundColor Gray
Invoke-RestMethod -Uri "$baseUrl/api/delete?key=test_100" -Method Delete
Write-Host "  + DELETE test_100" -ForegroundColor Green

# Verify deletion
try {
    Invoke-RestMethod -Uri "$baseUrl/api/get?key=test_100"
    Write-Host "  x ERROR: Key still exists!" -ForegroundColor Red
} catch {
    Write-Host "  + Verified: Key no longer exists" -ForegroundColor Green
}

# 9. Browse All Entries
Write-Host "`n[9] BROWSE ALL ENTRIES" -ForegroundColor Yellow
Write-Host "------------------------------------------------------------" -ForegroundColor Gray
$browseResponse = Invoke-RestMethod -Uri "$baseUrl/api/list?offset=0&limit=5"
Write-Host "  First 5 entries (paginated):" -ForegroundColor Cyan
foreach ($entry in $browseResponse.entries) {
    Write-Host "    $($entry.key) = $($entry.value)" -ForegroundColor Gray
}

# Final Stats
Write-Host "`n[10] FINAL DATABASE STATE" -ForegroundColor Yellow
Write-Host "------------------------------------------------------------" -ForegroundColor Gray
$finalStats = Invoke-RestMethod -Uri "$baseUrl/api/stats"
Write-Host "  Total Entries:    " -NoNewline -ForegroundColor Cyan
Write-Host $finalStats.total_entries -ForegroundColor White
Write-Host "  Uptime:           " -NoNewline -ForegroundColor Cyan
Write-Host "$([math]::Round($finalStats.uptime_seconds, 1))s" -ForegroundColor White

# Interactive Features Guide
Write-Host "`n===================================================================" -ForegroundColor Cyan
Write-Host "  WEB UI FEATURES (Open http://localhost:8080 in browser)" -ForegroundColor Cyan
Write-Host "===================================================================" -ForegroundColor Cyan

Write-Host "`nTABBED INTERFACE:" -ForegroundColor Yellow
Write-Host "  • Operations Tab:   PUT/GET/DELETE with visual feedback" -ForegroundColor White
Write-Host "  • Batch Ops Tab:    Generate bulk data, batch GET" -ForegroundColor White
Write-Host "  • Browse Tab:       Paginated table view, search filter" -ForegroundColor White
Write-Host "  • Range Scan Tab:   Scan key ranges with limit" -ForegroundColor White
Write-Host "  • Vector Tab:       Add vectors, similarity search (128D)" -ForegroundColor White
Write-Host "  • Metrics Tab:      Real-time Prometheus metrics" -ForegroundColor White
Write-Host "  • Health Tab:       Component health checks" -ForegroundColor White
Write-Host "  • Files Tab:        Database file tree explorer" -ForegroundColor White

Write-Host "`nREAL-TIME UPDATES:" -ForegroundColor Yellow
Write-Host "  • Header shows live stats (entries, pages, hit rate)" -ForegroundColor White
Write-Host "  • Auto-refresh every 5 seconds" -ForegroundColor White
Write-Host "  • Visual feedback (success/error toasts)" -ForegroundColor White

Write-Host "`nADVANCED FEATURES:" -ForegroundColor Yellow
Write-Host "  • JSON syntax highlighting for values" -ForegroundColor White
Write-Host "  • Pagination for large datasets (50 entries/page)" -ForegroundColor White
Write-Host "  • Search/filter in browse tab" -ForegroundColor White
Write-Host "  • Export metrics to Prometheus" -ForegroundColor White
Write-Host "  • Component health monitoring" -ForegroundColor White

Write-Host "`n===================================================================" -ForegroundColor Cyan
Write-Host "  DEMONSTRATION COMPLETE" -ForegroundColor Cyan
Write-Host "===================================================================" -ForegroundColor Cyan

Write-Host "`nNEXT STEPS:" -ForegroundColor Yellow
Write-Host "  1. Open browser: http://localhost:8080" -ForegroundColor White
Write-Host "  2. Explore the 8 tabs in the web interface" -ForegroundColor White
Write-Host "  3. Try creating custom entries with JSON values" -ForegroundColor White
Write-Host "  4. Monitor metrics at http://localhost:8080/metrics" -ForegroundColor White
Write-Host "  5. Check health at http://localhost:8080/health`n" -ForegroundColor White
