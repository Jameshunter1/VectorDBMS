# LSM Database Engine - Simple Web Demo
# Run the web interface and open it in browser

Write-Host "`n=====================================" -ForegroundColor Cyan
Write-Host "LSM DATABASE ENGINE - WEB INTERFACE" -ForegroundColor Cyan
Write-Host "=====================================`n" -ForegroundColor Cyan

# Kill any existing instances
Stop-Process -Name "dbweb" -Force -ErrorAction SilentlyContinue

# Clean test directory
$testDir = ".\_web_demo"
if (Test-Path $testDir) {
    Remove-Item -Path $testDir -Recurse -Force
}

Write-Host "Starting web server on port 8080..." -ForegroundColor Yellow

# Start the web server
$webProcess = Start-Process -FilePath ".\build\windows-vs2022-x64-debug\Debug\dbweb.exe" `
    -ArgumentList $testDir, "8080" `
    -PassThru

# Wait for server to start
Start-Sleep -Seconds 2

Write-Host "Server started (PID: $($webProcess.Id))`n" -ForegroundColor Green

# Quick API test
Write-Host "Testing API..." -ForegroundColor Yellow
Start-Sleep -Seconds 1

# Insert test data
Invoke-RestMethod -Uri "http://127.0.0.1:8080/api/put" `
    -Method POST `
    -Body @{key="user_1"; value="Alice"} `
    -ContentType "application/x-www-form-urlencoded" | Out-Null

Write-Host "  - Added test entry: user_1=Alice" -ForegroundColor Gray

# Get it back
$value = Invoke-RestMethod -Uri "http://127.0.0.1:8080/api/get?key=user_1"
Write-Host "  - Retrieved: $value`n" -ForegroundColor Gray

Write-Host "=====================================" -ForegroundColor Cyan
Write-Host "WEB INTERFACE READY!" -ForegroundColor Green -BackgroundColor Black
Write-Host "=====================================" -ForegroundColor Cyan
Write-Host "`nURL: " -NoNewline
Write-Host "http://127.0.0.1:8080/" -ForegroundColor White -BackgroundColor Blue
Write-Host "`nOpening browser...`n" -ForegroundColor Gray

# Open in browser
Start-Process "http://127.0.0.1:8080/"

Write-Host "Press CTRL+C to stop the server" -ForegroundColor Yellow
Write-Host "(Server will keep running in background)`n" -ForegroundColor Gray

# Wait for user to stop
try {
    Wait-Process -Id $webProcess.Id
} catch {
    # User pressed Ctrl+C
}
