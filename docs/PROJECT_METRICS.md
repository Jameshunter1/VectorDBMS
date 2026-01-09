# Project Metrics (as of 2026-01-09)

- C/C++ source and header files: 56
- Total files (excluding build/_deps): 120
- Total folders (excluding build/_deps): 47
- Markdown files: 18
- README.md files: 5
- Lines of C/C++ code: 10,965

## How to Update

To keep this file up to date, run the following PowerShell commands from the project root:


# C/C++ source and header files (excluding build/_deps)
`(Get-ChildItem -Recurse -Include *.cpp,*.hpp,*.h,*.c -File | Where-Object { $_.FullName -notmatch '\\build\\|_deps\\' } | Measure-Object).Count`

# Total files (excluding build/_deps)
`(Get-ChildItem -Recurse -File | Where-Object { $_.FullName -notmatch '\\build\\|_deps\\' } | Measure-Object).Count`

# Total folders (excluding build/_deps)
`(Get-ChildItem -Recurse -Directory | Where-Object { $_.FullName -notmatch '\\build\\|_deps\\' } | Measure-Object).Count`

# Markdown files (excluding build/_deps)
`(Get-ChildItem -Recurse -Include *.md -File | Where-Object { $_.FullName -notmatch '\\build\\|_deps\\' } | Measure-Object).Count`

# README.md files (excluding build/_deps)
`(Get-ChildItem -Recurse -Include README.md -File | Where-Object { $_.FullName -notmatch '\\build\\|_deps\\' } | Measure-Object).Count`

# Lines of C/C++ code (excluding build/_deps)
`$files = Get-ChildItem -Recurse -Include *.cpp,*.hpp,*.h,*.c -File | Where-Object { $_.FullName -notmatch '\\build\\|_deps\\' }; $total = 0; foreach ($f in $files) { $total += (Get-Content $f | Measure-Object -Line).Lines }; $total`

