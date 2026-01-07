#!/usr/bin/env pwsh
# Demo script for enhanced LSM Database web interface

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  LSM Database - Enhanced Web Interface" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Paths
$ROOT = $PSScriptRoot
$DBWEB = Join-Path $ROOT "build\windows-vs2022-x64-debug\Debug\dbweb.exe"
$DEMO_DIR = Join-Path $ROOT "_web_demo"

# Kill any existing instances
Write-Host "[1/5] Stopping previous instances..." -ForegroundColor Yellow
Get-Process -Name "dbweb" -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1

# Clean and create demo directory
Write-Host "[2/5] Preparing database directory..." -ForegroundColor Yellow
if (Test-Path $DEMO_DIR) {
    Remove-Item -Path $DEMO_DIR -Recurse -Force
}
New-Item -ItemType Directory -Path $DEMO_DIR | Out-Null

# Check if executable exists
if (-not (Test-Path $DBWEB)) {
    Write-Host "ERROR: dbweb.exe not found. Please build first:" -ForegroundColor Red
    Write-Host "  cd build/windows-vs2022-x64-debug" -ForegroundColor Red
    Write-Host "  cmake --build . --config Debug --target dbweb" -ForegroundColor Red
    exit 1
}

# Start the web interface in background
Write-Host "[3/5] Starting enhanced web interface..." -ForegroundColor Yellow
$job = Start-Job -ScriptBlock {
    param($exe, $dir)
    & $exe $dir
} -ArgumentList $DBWEB, $DEMO_DIR

Start-Sleep -Seconds 2

# Test API endpoint
Write-Host "[4/5] Testing API..." -ForegroundColor Yellow
$url = "http://127.0.0.1:8080"
$maxRetries = 5
$retries = 0

while ($retries -lt $maxRetries) {
    try {
        $response = Invoke-RestMethod -Uri "$url/api/stats" -Method Get -TimeoutSec 2
        Write-Host "    API Status: OK" -ForegroundColor Green
        Write-Host "    MemTable Entries: $($response.memtable_entry_count)" -ForegroundColor Green
        Write-Host "    SSTable Count: $($response.sstable_count)" -ForegroundColor Green
        break
    }
    catch {
        $retries++
        if ($retries -eq $maxRetries) {
            Write-Host "    ERROR: API not responding after $maxRetries attempts" -ForegroundColor Red
            Stop-Job -Job $job
            Remove-Job -Job $job
            exit 1
        }
        Start-Sleep -Seconds 1
    }
}

# Open browser
Write-Host "[5/5] Opening browser..." -ForegroundColor Yellow
Start-Process $url

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "  Enhanced Web Interface Running!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Features:" -ForegroundColor Cyan
Write-Host "  Browse Data: Paginated view with search and filtering" -ForegroundColor White
Write-Host "  Statistics: Real-time metrics and level breakdown" -ForegroundColor White
Write-Host "  Files: View SSTable and WAL file organization" -ForegroundColor White
Write-Host "  Console: Live operation logging" -ForegroundColor White
Write-Host "  Export: Download all data as JSON" -ForegroundColor White
Write-Host ""
Write-Host "URL: $url" -ForegroundColor Yellow
Write-Host ""
Write-Host "Press Ctrl+C to stop the server" -ForegroundColor Gray

# Wait for job (keeps server running)
Wait-Job -Job $job | Out-Null
