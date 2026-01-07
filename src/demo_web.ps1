# LSM Database Engine - Web Interface Demo
# Shows the complete functionality with live testing

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "LSM DATABASE ENGINE - WEB INTERFACE DEMO" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

# Kill any existing web servers
Write-Host "[1/5] Cleaning up previous instances..." -ForegroundColor Yellow
Stop-Process -Name "dbweb" -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1

# Clean test directory
$testDir = ".\_web_demo"
if (Test-Path $testDir) {
    Remove-Item -Path $testDir -Recurse -Force
}

Write-Host "[2/5] Starting web interface on port 8080..." -ForegroundColor Yellow
$webProcess = Start-Process -FilePath ".\build\windows-vs2022-x64-debug\Debug\dbweb.exe" `
    -ArgumentList $testDir, "8080" `
    -PassThru `
    -WindowStyle Hidden

# Wait for server to start
Start-Sleep -Seconds 2

Write-Host "[3/5] Testing API endpoints..." -ForegroundColor Yellow

# Test 1: PUT operation
Write-Host "  Testing PUT..." -NoNewline
try {
    $body = @{
        key = "user_1"
        value = "Alice"
    }
    $response = Invoke-WebRequest -Uri "http://127.0.0.1:8080/api/put" `
        -Method POST `
        -Body $body `
        -ContentType "application/x-www-form-urlencoded" `
        -UseBasicParsing
    
    if ($response.StatusCode -eq 200) {
        Write-Host " ✓" -ForegroundColor Green
    }
} catch {
    Write-Host " ✗ Failed: $_" -ForegroundColor Red
}

# Test 2: GET operation
Write-Host "  Testing GET..." -NoNewline
try {
    $response = Invoke-WebRequest -Uri "http://127.0.0.1:8080/api/get?key=user_1" `
        -UseBasicParsing
    
    if ($response.StatusCode -eq 200 -and $response.Content -eq "Alice") {
        Write-Host " ✓ (value: $($response.Content))" -ForegroundColor Green
    }
} catch {
    Write-Host " ✗ Failed: $_" -ForegroundColor Red
}

# Test 3: Batch operations
Write-Host "  Testing batch insert (100 entries)..." -NoNewline
$startTime = Get-Date
for ($i = 0; $i -lt 100; $i++) {
    $body = @{
        key = "batch_$i"
        value = "data_$i"
    }
    Invoke-WebRequest -Uri "http://127.0.0.1:8080/api/put" `
        -Method POST `
        -Body $body `
        -ContentType "application/x-www-form-urlencoded" `
        -UseBasicParsing | Out-Null
}
$duration = (Get-Date) - $startTime
$opsPerSec = [math]::Round(100 / $duration.TotalSeconds, 0)
$message = " checkmark ($opsPerSec ops per sec)"
Write-Host $message -ForegroundColor Green

# Test 4: Stats
Write-Host "  Testing STATS..." -NoNewline
try {
    $response = Invoke-WebRequest -Uri "http://127.0.0.1:8080/api/stats" `
        -UseBasicParsing
    
    if ($response.StatusCode -eq 200) {
        $stats = $response.Content | ConvertFrom-Json
        Write-Host " ✓" -ForegroundColor Green
        Write-Host "    - Total puts: $($stats.total_puts)" -ForegroundColor Gray
        Write-Host "    - MemTable: $([math]::Round($stats.memtable_size_bytes / 1024, 1)) KB" -ForegroundColor Gray
        Write-Host "    - Entries: $($stats.memtable_entry_count)" -ForegroundColor Gray
    }
} catch {
    Write-Host " ✗ Failed: $_" -ForegroundColor Red
}

# Test 5: DELETE operation
Write-Host "  Testing DELETE..." -NoNewline
try {
    $body = @{
        key = "user_1"
    }
    $response = Invoke-WebRequest -Uri "http://127.0.0.1:8080/api/delete" `
        -Method POST `
        -Body $body `
        -ContentType "application/x-www-form-urlencoded" `
        -UseBasicParsing
    
    if ($response.StatusCode -eq 200) {
        # Verify it's gone
        try {
            Invoke-WebRequest -Uri "http://127.0.0.1:8080/api/get?key=user_1" `
                -UseBasicParsing | Out-Null
            Write-Host " ✗ Key still exists" -ForegroundColor Red
        } catch {
            Write-Host " ✓ (verified deleted)" -ForegroundColor Green
        }
    }
} catch {
    Write-Host " ✗ Failed: $_" -ForegroundColor Red
}

Write-Host "`n[4/5] Analyzing database files..." -ForegroundColor Yellow
if (Test-Path $testDir) {
    $walSize = (Get-Item "$testDir\wal.log").Length
    Write-Host "  WAL size: $([math]::Round($walSize / 1024, 1)) KB" -ForegroundColor Gray
    
    if (Test-Path "$testDir\level_0") {
        $level0Files = (Get-ChildItem "$testDir\level_0" -Filter "*.sst").Count
        Write-Host "  Level 0 files: $level0Files" -ForegroundColor Gray
    }
    
    if (Test-Path "$testDir\level_1") {
        $level1Files = (Get-ChildItem "$testDir\level_1" -Filter "*.sst").Count
        Write-Host "  Level 1 files: $level1Files" -ForegroundColor Gray
    }
}

Write-Host "`n[5/5] Opening web interface..." -ForegroundColor Yellow
Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "WEB INTERFACE READY!" -ForegroundColor Green
Write-Host "========================================`n" -ForegroundColor Cyan
Write-Host "URL: http://127.0.0.1:8080/" -ForegroundColor White -BackgroundColor Blue
Write-Host "`nThe web interface will open in your browser..." -ForegroundColor Gray
Write-Host "Press Ctrl+C to stop the server when done.`n" -ForegroundColor Gray

# Open in default browser
Start-Process "http://127.0.0.1:8080/"

# Keep server running
Write-Host "Server is running (PID: $($webProcess.Id))..." -ForegroundColor Yellow
Write-Host "Press any key to stop..." -ForegroundColor Yellow
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

# Cleanup
Write-Host "`nStopping server..." -ForegroundColor Yellow
Stop-Process -Id $webProcess.Id -Force -ErrorAction SilentlyContinue
Write-Host "Demo complete!`n" -ForegroundColor Green
