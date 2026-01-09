# Update project metrics and format code

# Run clang-format on all C/C++ files (uses format.ps1)
Write-Host "Running clang-format..."
.\format.ps1

# Calculate metrics
Write-Host "Calculating project metrics..."

$metricsPath = "PROJECT_METRICS.md"
$date = Get-Date -Format 'yyyy-MM-dd'

# C/C++ source and header files
$cppCount = (Get-ChildItem -Recurse -Include *.cpp,*.hpp,*.h,*.c -File | Where-Object { $_.FullName -notmatch '\\build\\|_deps\\' } | Measure-Object).Count

# Total files
$fileCount = (Get-ChildItem -Recurse -File | Where-Object { $_.FullName -notmatch '\\build\\|_deps\\' } | Measure-Object).Count

# Total folders
$folderCount = (Get-ChildItem -Recurse -Directory | Where-Object { $_.FullName -notmatch '\\build\\|_deps\\' } | Measure-Object).Count

# Markdown files
$mdCount = (Get-ChildItem -Recurse -Include *.md -File | Where-Object { $_.FullName -notmatch '\\build\\|_deps\\' } | Measure-Object).Count

# README.md files
$readmeCount = (Get-ChildItem -Recurse -Include README.md -File | Where-Object { $_.FullName -notmatch '\\build\\|_deps\\' } | Measure-Object).Count

# Lines of C/C++ code
$files = Get-ChildItem -Recurse -Include *.cpp,*.hpp,*.h,*.c -File | Where-Object { $_.FullName -notmatch '\\build\\|_deps\\' }
$loc = 0
foreach ($f in $files) { $loc += (Get-Content $f | Measure-Object -Line).Lines }

# Write metrics to file
@"
# Project Metrics (as of $date)

- C/C++ source and header files: $cppCount
- Total files (excluding build/_deps): $fileCount
- Total folders (excluding build/_deps): $folderCount
- Markdown files: $mdCount
- README.md files: $readmeCount
- Lines of C/C++ code: $loc

## How to Update

Run .\update_metrics.ps1 to refresh this file and reformat code.
"@ | Set-Content $metricsPath

Write-Host "Metrics updated in $metricsPath. Formatting complete."
