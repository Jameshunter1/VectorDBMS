# Check if all C++ files are properly formatted (CI validation)
# Usage: .\format-check.ps1

Write-Host "`n🔍 VectorDBMS Format Checker" -ForegroundColor Cyan
Write-Host "=" * 60 -ForegroundColor Cyan

# Check if clang-format is available
$clangFormat = $null
$clangFormatVersions = @("clang-format", "clang-format-17", "clang-format-16", "clang-format-15")

foreach ($version in $clangFormatVersions) {
    try {
        $output = & $version --version 2>&1
        if ($LASTEXITCODE -eq 0) {
            $clangFormat = $version
            Write-Host "`n✅ Found: $version" -ForegroundColor Green
            break
        }
    } catch {
        continue
    }
}

if (-not $clangFormat) {
    Write-Host "`n❌ clang-format not found!" -ForegroundColor Red
    exit 1
}

# Check all source files
Write-Host "`n📁 Checking formatting..." -ForegroundColor Yellow
Write-Host "   Using: .clang-format in repository root" -ForegroundColor Gray

$directories = @("src\include", "src\lib", "src\apps", "tests", "benchmarks")
$totalFiles = 0
$issues = @()

foreach ($dir in $directories) {
    if (Test-Path $dir) {
        Write-Host "`n  Checking $dir..." -ForegroundColor Cyan
        $files = Get-ChildItem -Path $dir -Recurse -Include *.cpp,*.hpp,*.h
        
        foreach ($file in $files) {
            $totalFiles++
            try {
                # Check if file needs formatting (dry-run)
                $output = & $clangFormat --dry-run --Werror -style=file $file.FullName 2>&1
                if ($LASTEXITCODE -ne 0) {
                    $issues += $file.Name
                    Write-Host "    ✗ $($file.Name)" -ForegroundColor Red
                } else {
                    Write-Host "    ✓ $($file.Name)" -ForegroundColor Green
                }
            } catch {
                Write-Host "    ⚠ $($file.Name): $_" -ForegroundColor Yellow
            }
        }
    }
}

Write-Host "`n" -NoNewline
if ($issues.Count -eq 0) {
    Write-Host "✅ All $totalFiles files are properly formatted!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "❌ Found $($issues.Count) file(s) with formatting issues:" -ForegroundColor Red
    foreach ($issue in $issues) {
        Write-Host "   • $issue" -ForegroundColor Yellow
    }
    Write-Host "`n💡 Run .\format.ps1 to fix formatting automatically" -ForegroundColor Cyan
    exit 1
}
