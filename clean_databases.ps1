# clean_databases.ps1
#
# Utility script to clean all database directories
# Use this if you encounter checksum corruption errors after code changes

Write-Host "`n╔══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  Vectis Database Cleanup Utility                       ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

$patterns = @(
    ".\build\**\*test*db*",
    ".\build\**\*tutorial*db*",
    "$env:TEMP\core_engine_*",
    ".\tutorial_*db*",
    ".\_data",
    ".\_dev_data",
    ".\test_*_db"
)

$cleaned = 0
$totalSize = 0

Write-Host "Scanning for database directories..." -ForegroundColor Yellow

foreach ($pattern in $patterns) {
    Get-ChildItem -Path $pattern -Directory -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
        try {
            $size = (Get-ChildItem -Path $_.FullName -Recurse -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum
            $totalSize += $size
            Remove-Item $_.FullName -Recurse -Force -ErrorAction Stop
            Write-Host "  ✓ Removed: $($_.Name) ($([math]::Round($size/1KB, 2)) KB)" -ForegroundColor Green
            $cleaned++
        } catch {
            Write-Host "  ✗ Failed to remove: $($_.FullName)" -ForegroundColor Red
        }
    }
}

Write-Host ""
if ($cleaned -eq 0) {
    Write-Host "No database directories found - already clean!" -ForegroundColor Cyan
} else {
    Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Green
    Write-Host "  Cleaned: $cleaned directories" -ForegroundColor Green
    Write-Host "  Freed:   $([math]::Round($totalSize/1MB, 2)) MB" -ForegroundColor Green
    Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Green
}

Write-Host "`nDatabase cleanup complete! You can now run tests with fresh databases.`n" -ForegroundColor Cyan
