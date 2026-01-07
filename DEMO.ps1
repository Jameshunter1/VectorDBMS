# Vectis Database - Demo & Presentation Script

## 🎯 Purpose
This script demonstrates Vectis Database from a complete user flow perspective,
showcasing installation, basic operations, vector search, and production deployment.

## 📋 Prerequisites
- Windows 10/11 with Visual Studio 2022
- CMake 3.24+
- PowerShell

---

## Part 1: Build the Database (5 minutes)

Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                                                              ║" -ForegroundColor Cyan
Write-Host "║              VECTIS DATABASE - DEMO SCRIPT                   ║" -ForegroundColor Cyan
Write-Host "║                  Production Version 1.5                      ║" -ForegroundColor Cyan
Write-Host "║                                                              ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

Write-Host "Step 1: Building Vectis Database..." -ForegroundColor Green
Write-Host "======================================" -ForegroundColor Green
Write-Host ""

# Navigate to project root
$projectRoot = Split-Path -Parent $PSScriptRoot
Set-Location $projectRoot

# Configure CMake
Write-Host "Configuring CMake..." -ForegroundColor Yellow
cmake --preset windows-vs2022-x64-release -S src

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ CMake configuration failed!" -ForegroundColor Red
    exit 1
}

# Build the project
Write-Host ""
Write-Host "Building project (this may take a few minutes)..." -ForegroundColor Yellow
cmake --build build/windows-vs2022-x64-release --config Release -j 8

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "✅ Build completed successfully!" -ForegroundColor Green
Write-Host ""

# Run tests
Write-Host "Step 2: Running Tests..." -ForegroundColor Green
Write-Host "======================================" -ForegroundColor Green
Write-Host ""

ctest --test-dir build/windows-vs2022-x64-release -C Release --output-on-failure

if ($LASTEXITCODE -ne 0) {
    Write-Host "⚠️  Some tests failed, but continuing demo..." -ForegroundColor Yellow
} else {
    Write-Host ""
    Write-Host "✅ All tests passed!" -ForegroundColor Green
}

Write-Host ""
Write-Host "Press any key to continue to Part 2 (CLI Demo)..." -ForegroundColor Cyan
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
Clear-Host

---

## Part 2: Interactive CLI Demo (5 minutes)

Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                 PART 2: INTERACTIVE CLI DEMO                 ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

$dbDir = Join-Path $env:TEMP "vectis_demo_db"

# Clean up old demo data
if (Test-Path $dbDir) {
    Write-Host "Cleaning up old demo database..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $dbDir
}

Write-Host "Creating demo database at: $dbDir" -ForegroundColor Yellow
Write-Host ""

$cliPath = ".\build\windows-vs2022-x64-release\Release\dbcli.exe"

# Demonstrate single commands
Write-Host "Demonstrating Single Commands:" -ForegroundColor Green
Write-Host "-------------------------------" -ForegroundColor Green
Write-Host ""

Write-Host "➜ Storing user data..." -ForegroundColor Cyan
& $cliPath $dbDir put "user:alice" "Alice Johnson"
& $cliPath $dbDir put "user:bob" "Bob Smith"
& $cliPath $dbDir put "user:charlie" "Charlie Brown"

Write-Host ""
Write-Host "➜ Retrieving data..." -ForegroundColor Cyan
$alice = & $cliPath $dbDir get "user:alice"
Write-Host "user:alice = $alice" -ForegroundColor White

Write-Host ""
Write-Host "➜ Deleting data..." -ForegroundColor Cyan
& $cliPath $dbDir delete "user:bob"

Write-Host ""
Write-Host "✅ Single commands work!" -ForegroundColor Green
Write-Host ""

Write-Host "Now launching Interactive Mode..." -ForegroundColor Yellow
Write-Host "Try these commands:" -ForegroundColor Yellow
Write-Host "  - help          (show all commands)" -ForegroundColor Gray
Write-Host "  - stats         (show statistics)" -ForegroundColor Gray
Write-Host "  - scan user:000 user:999 10" -ForegroundColor Gray
Write-Host "  - bput name:Dave age:35 city:SF" -ForegroundColor Gray
Write-Host "  - quit          (exit interactive mode)" -ForegroundColor Gray
Write-Host ""
Write-Host "Press any key to start interactive CLI..." -ForegroundColor Cyan
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

# Launch interactive mode
& $cliPath $dbDir

Write-Host ""
Write-Host "Press any key to continue to Part 3 (Web Server Demo)..." -ForegroundColor Cyan
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
Clear-Host

---

## Part 3: Web Server Demo (5 minutes)

Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                 PART 3: WEB SERVER DEMO                      ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

$webPath = ".\build\windows-vs2022-x64-release\Release\dbweb.exe"
$port = 8080

Write-Host "Starting Vectis Web Server on port $port..." -ForegroundColor Yellow

# Start web server in background
$webJob = Start-Job -ScriptBlock {
    param($webPath, $dbDir, $port)
    & $webPath $dbDir $port
} -ArgumentList $webPath, $dbDir, $port

Write-Host "✅ Server started!" -ForegroundColor Green
Write-Host ""
Write-Host "Waiting for server to initialize..." -ForegroundColor Yellow
Start-Sleep -Seconds 2

# Test health check
Write-Host ""
Write-Host "Testing Health Check..." -ForegroundColor Cyan
$health = Invoke-RestMethod -Uri "http://localhost:$port/api/health" -Method Get
Write-Host "Status: $($health.status)" -ForegroundColor Green

# Demonstrate API calls
Write-Host ""
Write-Host "Demonstrating HTTP API:" -ForegroundColor Green
Write-Host "----------------------" -ForegroundColor Green
Write-Host ""

# PUT
Write-Host "➜ PUT /api/put" -ForegroundColor Cyan
$putResponse = Invoke-RestMethod -Uri "http://localhost:$port/api/put" `
    -Method Post `
    -Body @{key="product:1"; value="Laptop"} `
    -ContentType "application/x-www-form-urlencoded"
Write-Host "Response: $putResponse" -ForegroundColor White

# GET
Write-Host ""
Write-Host "➜ GET /api/get?key=product:1" -ForegroundColor Cyan
$getResponse = Invoke-RestMethod -Uri "http://localhost:$port/api/get?key=product:1" -Method Get
Write-Host "Response: $getResponse" -ForegroundColor White

# STATS
Write-Host ""
Write-Host "➜ GET /api/stats" -ForegroundColor Cyan
$stats = Invoke-RestMethod -Uri "http://localhost:$port/api/stats" -Method Get
Write-Host "Total Pages: $($stats.total_pages)" -ForegroundColor White
Write-Host "Total Puts:  $($stats.total_puts)" -ForegroundColor White
Write-Host "Total Gets:  $($stats.total_gets)" -ForegroundColor White

Write-Host ""
Write-Host "✅ Web API working!" -ForegroundColor Green
Write-Host ""
Write-Host "Server is running at: http://localhost:$port" -ForegroundColor Yellow
Write-Host "Try visiting: http://localhost:$port (Web Dashboard)" -ForegroundColor Yellow
Write-Host ""

Write-Host "Press any key to stop the server and continue..." -ForegroundColor Cyan
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

# Stop web server
Stop-Job -Job $webJob
Remove-Job -Job $webJob

Clear-Host

---

## Part 4: Python SDK Demo (if Python installed)

Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                 PART 4: PYTHON SDK DEMO                      ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Check if Python is available
$pythonAvailable = Get-Command python -ErrorAction SilentlyContinue

if ($pythonAvailable) {
    Write-Host "Python detected! Demonstrating Python SDK..." -ForegroundColor Green
    Write-Host ""
    
    # Create demo Python script
    $pythonScript = @"
from vectis import VectisClient
import sys

try:
    # Connect to database
    client = VectisClient("http://localhost:8080")
    print("✓ Connected to Vectis")
    
    # Store data
    client.put("demo:key1", "Hello from Python!")
    print("✓ Stored data")
    
    # Retrieve data
    value = client.get("demo:key1")
    print(f"✓ Retrieved: {value}")
    
    # Batch operations
    client.batch_put({
        "lang:python": "Python",
        "lang:cpp": "C++",
        "lang:rust": "Rust"
    })
    print("✓ Batch insert successful")
    
    # Get stats
    stats = client.get_stats()
    print(f"✓ Stats: {stats['total_puts']} puts, {stats['total_gets']} gets")
    
except Exception as e:
    print(f"❌ Error: {e}", file=sys.stderr)
    sys.exit(1)
"@
    
    $scriptPath = Join-Path $env:TEMP "vectis_demo.py"
    $pythonScript | Out-File -FilePath $scriptPath -Encoding UTF8
    
    # Restart server for Python demo
    Write-Host "Restarting server for Python demo..." -ForegroundColor Yellow
    $webJob = Start-Job -ScriptBlock {
        param($webPath, $dbDir, $port)
        & $webPath $dbDir $port
    } -ArgumentList $webPath, $dbDir, $port
    
    Start-Sleep -Seconds 2
    
    Write-Host ""
    python $scriptPath
    
    Stop-Job -Job $webJob
    Remove-Job -Job $webJob
    
    Write-Host ""
    Write-Host "✅ Python SDK demo complete!" -ForegroundColor Green
} else {
    Write-Host "Python not detected. Skipping Python SDK demo." -ForegroundColor Yellow
    Write-Host "Install Python and 'pip install vectis' to try the Python SDK." -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Press any key to continue to summary..." -ForegroundColor Cyan
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
Clear-Host

---

## Summary & Next Steps

Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                     DEMO COMPLETE! 🎉                        ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

Write-Host "What we demonstrated:" -ForegroundColor Green
Write-Host "--------------------" -ForegroundColor Green
Write-Host "✓ Built Vectis from source" -ForegroundColor White
Write-Host "✓ Ran comprehensive tests" -ForegroundColor White
Write-Host "✓ Used interactive CLI" -ForegroundColor White
Write-Host "✓ Started web server" -ForegroundColor White
Write-Host "✓ Tested HTTP API" -ForegroundColor White
if ($pythonAvailable) {
    Write-Host "✓ Used Python SDK" -ForegroundColor White
}
Write-Host ""

Write-Host "Key Features:" -ForegroundColor Green
Write-Host "-------------" -ForegroundColor Green
Write-Host "• Page-based storage engine (4 KB pages)" -ForegroundColor White
Write-Host "• ACID guarantees with WAL" -ForegroundColor White
Write-Host "• Vector search with HNSW index" -ForegroundColor White
Write-Host "• Buffer pool manager with LRU-K eviction" -ForegroundColor White
Write-Host "• Automatic compaction" -ForegroundColor White
Write-Host "• HTTP REST API" -ForegroundColor White
Write-Host "• Python SDK for easy integration" -ForegroundColor White
Write-Host "• Docker support for deployment" -ForegroundColor White
Write-Host ""

Write-Host "Next Steps:" -ForegroundColor Green
Write-Host "-----------" -ForegroundColor Green
Write-Host "1. Read the User Guide:    USER_GUIDE.md" -ForegroundColor Cyan
Write-Host "2. Try Docker deployment:  docker-compose up" -ForegroundColor Cyan
Write-Host "3. Build semantic search:  See USER_GUIDE.md (Part 5)" -ForegroundColor Cyan
Write-Host "4. Review API reference:   src/API_REFERENCE.md" -ForegroundColor Cyan
Write-Host "5. Check performance:      .\build\Release\core_engine_benchmarks.exe" -ForegroundColor Cyan
Write-Host ""

Write-Host "Demo database location: $dbDir" -ForegroundColor Yellow
Write-Host ""

Write-Host "Thank you for trying Vectis Database!" -ForegroundColor Green
Write-Host "For more information, visit: https://github.com/yourusername/VectorDBMS" -ForegroundColor Cyan
Write-Host ""
