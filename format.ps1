# Auto-format all C++ source files with clang-format
# Usage: .\format.ps1

Write-Host "`nVectorDBMS Code Formatter" -ForegroundColor Cyan
Write-Host ("=" * 60) -ForegroundColor Cyan

# Check if clang-format is available
$clangFormat = $null
$clangFormatVersions = @("clang-format", "clang-format-17", "clang-format-16", "clang-format-15")

foreach ($version in $clangFormatVersions) {
    try {
        $output = & $version --version 2>&1
        if ($LASTEXITCODE -eq 0) {
            $clangFormat = $version
            Write-Host "`nFound: $version" -ForegroundColor Green
            Write-Host "   $output" -ForegroundColor Gray
            break
        }
    } catch {
        continue
    }
}

if (-not $clangFormat) {
    Write-Host "`nERROR: clang-format not found!" -ForegroundColor Red
    Write-Host "`nPlease install LLVM/Clang:" -ForegroundColor Yellow
    Write-Host "  - Windows: winget install LLVM.LLVM" -ForegroundColor Cyan
    Write-Host "  - Or download from: https://releases.llvm.org/" -ForegroundColor Cyan
    Write-Host "`nAfter installation, restart your terminal." -ForegroundColor Yellow
    exit 1
}

# Format all source files
Write-Host "`nFormatting source files..." -ForegroundColor Yellow
Write-Host "   Using: .clang-format in repository root" -ForegroundColor Gray

$directories = @("src\include", "src\lib", "src\apps", "tests", "benchmarks")
$totalFiles = 0
$formattedFiles = 0

foreach ($dir in $directories) {
    if (Test-Path $dir) {
        Write-Host "`n  Processing $dir..." -ForegroundColor Cyan
        $files = Get-ChildItem -Path $dir -Recurse -Include *.cpp,*.hpp,*.h
        
        foreach ($file in $files) {
            $totalFiles++
            try {
                & $clangFormat -i -style=file $file.FullName
                $formattedFiles++
                Write-Host "    [OK] $($file.Name)" -ForegroundColor Green
            } catch {
                Write-Host "    [FAIL] $($file.Name): $_" -ForegroundColor Red
            }
        }
    }
}

Write-Host "`nFormatted $formattedFiles of $totalFiles files successfully" -ForegroundColor Green
Write-Host "`nTip: Add clang-format to your pre-commit hook for automatic formatting" -ForegroundColor Cyan
