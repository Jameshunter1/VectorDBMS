# Generate Test Data for VectorDBMS + Grafana
# This script populates the database with sample data and verifies metrics

Write-Host "`n🚀 VectorDBMS Test Data Generator" -ForegroundColor Cyan
Write-Host "=" * 60 -ForegroundColor Cyan

# Check if database is running
Write-Host "`n📡 Checking database connection..." -ForegroundColor Yellow
try {
    $response = Invoke-RestMethod -Uri "http://localhost:8080/api/stats" -TimeoutSec 5
    Write-Host "✅ Database is online!" -ForegroundColor Green
} catch {
    Write-Host "❌ Database is not responding. Please start with: docker compose up -d" -ForegroundColor Red
    exit 1
}

# Generate test data
Write-Host "`n📊 Generating 100 test entries..." -ForegroundColor Yellow
$successCount = 0
$errorCount = 0

for ($i = 1; $i -le 100; $i++) {
    try {
        $null = Invoke-RestMethod -Uri "http://localhost:8080/api/put?key=user_$i&value=data_$i" -Method Get -TimeoutSec 2
        $successCount++
        
        if ($i % 20 -eq 0) {
            Write-Host "  ✓ Created $i entries..." -ForegroundColor Green
        }
    } catch {
        $errorCount++
    }
}

Write-Host "✅ Generated $successCount entries ($errorCount errors)" -ForegroundColor Green

# Get updated stats
Write-Host "`n📈 Current Database Stats:" -ForegroundColor Yellow
$stats = Invoke-RestMethod -Uri "http://localhost:8080/api/stats"
Write-Host "  Total Pages:    $($stats.total_pages)" -ForegroundColor Cyan
Write-Host "  Total Writes:   $($stats.total_writes)" -ForegroundColor Cyan
Write-Host "  Total Reads:    $($stats.total_reads)" -ForegroundColor Cyan
Write-Host "  Total PUTs:     $($stats.total_puts)" -ForegroundColor Cyan
Write-Host "  Total GETs:     $($stats.total_gets)" -ForegroundColor Cyan
Write-Host "  Avg PUT (μs):   $($stats.avg_put_time_us)" -ForegroundColor Cyan
Write-Host "  Avg GET (μs):   $($stats.avg_get_time_us)" -ForegroundColor Cyan

# Check metrics endpoint
Write-Host "`n🔍 Checking Prometheus metrics endpoint..." -ForegroundColor Yellow
try {
    $metricsResponse = Invoke-RestMethod -Uri "http://localhost:8080/metrics" -TimeoutSec 5
    if ($metricsResponse -match "core_engine") {
        Write-Host "✅ Metrics endpoint is working!" -ForegroundColor Green
        Write-Host "  Sample metrics:" -ForegroundColor Cyan
        $metricsResponse -split "`n" | Where-Object { $_ -match "core_engine_total" } | Select-Object -First 5 | ForEach-Object {
            Write-Host "    $_" -ForegroundColor Gray
        }
    } else {
        Write-Host "⚠️  Metrics endpoint returned data but no core_engine metrics found" -ForegroundColor Yellow
    }
} catch {
    Write-Host "❌ Metrics endpoint not available (404). Rebuild Docker image to add it." -ForegroundColor Red
}

# Verify Prometheus can scrape
Write-Host "`n🔎 Checking Prometheus scraping status..." -ForegroundColor Yellow
try {
    $targets = Invoke-RestMethod -Uri "http://localhost:9090/api/v1/targets" -TimeoutSec 5
    $vectisTarget = $targets.data.activeTargets | Where-Object { $_.labels.job -eq "vectis" }
    
    if ($vectisTarget) {
        $health = $vectisTarget.health
        $lastScrape = $vectisTarget.lastScrape
        
        if ($health -eq "up") {
            Write-Host "✅ Prometheus is successfully scraping VectorDBMS!" -ForegroundColor Green
            Write-Host "  Last scrape: $lastScrape" -ForegroundColor Cyan
        } else {
            Write-Host "⚠️  Prometheus target is $health" -ForegroundColor Yellow
            Write-Host "  Last error: $($vectisTarget.lastError)" -ForegroundColor Red
        }
    } else {
        Write-Host "⚠️  VectorDBMS target not found in Prometheus" -ForegroundColor Yellow
    }
} catch {
    Write-Host "⚠️  Could not connect to Prometheus at localhost:9090" -ForegroundColor Yellow
}

# Test some GETs
Write-Host "`n🔄 Testing GET operations..." -ForegroundColor Yellow
$getCount = 0
for ($i = 1; $i -le 20; $i++) {
    try {
        $null = Invoke-RestMethod -Uri "http://localhost:8080/api/get?key=user_$i" -TimeoutSec 2
        $getCount++
    } catch {
        # Ignore errors
    }
}
Write-Host "✅ Successfully retrieved $getCount values" -ForegroundColor Green

# Final stats
Write-Host "`n📊 Final Statistics:" -ForegroundColor Yellow
$finalStats = Invoke-RestMethod -Uri "http://localhost:8080/api/stats"
Write-Host "  Total Operations: $($finalStats.total_puts + $finalStats.total_gets)" -ForegroundColor Cyan

# Open dashboards
Write-Host "`n🌐 Opening monitoring dashboards..." -ForegroundColor Yellow
Write-Host "  Database:  http://localhost:8080" -ForegroundColor Cyan
Write-Host "  Prometheus: http://localhost:9090" -ForegroundColor Cyan
Write-Host "  Grafana:    http://localhost:3000 (admin/admin)" -ForegroundColor Cyan

Write-Host "`n✨ Test data generation complete!" -ForegroundColor Green
Write-Host "=" * 60 -ForegroundColor Cyan
Write-Host ""
