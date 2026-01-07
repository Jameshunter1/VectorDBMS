# Code Formatting Guide

VectorDBMS uses **clang-format** with LLVM style for consistent C++ formatting.

## Quick Start

### Install clang-format

**Windows:**
```powershell
winget install LLVM.LLVM
```

**Linux/macOS:**
```bash
sudo apt-get install clang-format-17  # Ubuntu/Debian
brew install clang-format              # macOS
```

### Format All Code

```powershell
.\format.ps1          # Auto-format all C++ files
.\format-check.ps1    # Check formatting (CI validation)
```

## Configuration

The [.clang-format](.clang-format) file in the repository root defines our style:

- **Base Style**: LLVM
- **Indent**: 2 spaces
- **Line Limit**: 100 characters
- **Pointer Alignment**: Left (`int* ptr`)
- **Sort Includes**: Enabled

## CI Integration

The GitHub Actions CI automatically checks formatting on all PRs. To pass:

1. Install clang-format locally
2. Run `.\format.ps1` before committing
3. Or configure your IDE to format-on-save

## IDE Setup

### Visual Studio Code
Install the [C/C++ extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) and add to `.vscode/settings.json`:

```json
{
  "editor.formatOnSave": true,
  "C_Cpp.clang_format_style": "file"
}
```

### Visual Studio
Tools → Options → Text Editor → C/C++ → Formatting:
- ✓ Enable ClangFormat support
- Style: Use .clang-format file

### CLion
Settings → Editor → Code Style → C/C++:
- Scheme: Project
- ✓ Enable ClangFormat with clangd
