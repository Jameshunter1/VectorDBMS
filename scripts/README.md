# Scripts Directory

This folder contains utility scripts for development, testing, and demonstration.

## Demo Scripts

- **demo_frontend.ps1** - Start the web server and open browser to UI
- **demo_vectors.ps1** - Populate database with vector embeddings
- **demo_vectors_simple.ps1** - Simple vector insertion demo (20 vectors)
- **demo_web_frontend.ps1** - Alternative web frontend launcher

## Development Scripts

- **generate_test_data.ps1** - Generate test data for benchmarking

## Usage

### PowerShell (Windows)
```powershell
# Run a demo script
.\scripts\demo_vectors_simple.ps1
```

### Bash (Linux/macOS with PowerShell Core)
```bash
# Install PowerShell Core if needed
# https://docs.microsoft.com/en-us/powershell/scripting/install/installing-powershell

pwsh scripts/demo_vectors_simple.ps1
```

## Notes

- Scripts assume database server is running on `http://localhost:8080`
- Demo scripts create temporary databases in the working directory
- See main README.md for Docker-based deployment
