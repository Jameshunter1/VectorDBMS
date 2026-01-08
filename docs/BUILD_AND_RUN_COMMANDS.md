# Build & Run Reference

Use these commands from the repository root to configure, build, and run the dbweb frontend/server.

1. Configure the CMake project (generate VS 2022 Debug files):

   ```powershell
   cmake --preset windows-vs2022-x64-debug
   ```

2. Build the `dbweb` executable (Debug config):

   ```powershell
   cmake --build build/windows-vs2022-x64-debug --config Debug --target dbweb
   ```

3. Run the web server (defaults: data dir `./_web_demo`, port `8080`, vector dim `128`):

   ```powershell
   .\build\windows-vs2022-x64-debug\Debug\dbweb.exe [optional_db_dir] [optional_port] [optional_vector_dim]
   ```

   Passing a third argument lets you change the required vector dimension (e.g., `256` for 256-D embeddings).

4. Smoke-test the endpoints from another shell to verify the UI backend is alive:

   ```powershell
   Invoke-WebRequest -UseBasicParsing http://localhost:8080/api/stats | Select-Object -ExpandProperty Content
   Invoke-WebRequest -UseBasicParsing http://localhost:8080/api/vector/list | Select-Object -ExpandProperty Content
   ```

   On macOS/Linux the same verification can be done with curl:

   ```bash
   curl http://localhost:8080/api/stats
   curl http://localhost:8080/api/vector/list
   ```

5. Stop a previously running instance (if the EXE is locked during rebuilds):

   ```powershell
   Get-Process dbweb -ErrorAction SilentlyContinue | Stop-Process
   ```

6. (Optional) Reformat C++ sources before committing:

   ```powershell
   .\format.ps1
   ```

## Docker Workflows

Use these commands when you want to run the database inside containers instead of building locally.

- **Build Linux image** (multi-stage `Dockerfile`):
   ```bash
   docker build -t vectis:latest .
   ```

- **Run single container** (persists data to a named volume and exports port 8080):
   ```bash
   docker run -d \
      --name vectis-db \
      -p 8080:8080 \
      -v vectis-data:/vectis/data \
      -e VECTIS_BUFFER_POOL_SIZE_MB=256 \
      vectis:latest
   ```

- **Launch monitoring stack** (Compose brings up Vectis + Prometheus + Grafana):
   ```bash
   docker compose up -d
   ```

- **Build Windows container** (uses `Dockerfile.windows` on hosts with Windows containers enabled):
   ```powershell
   docker build -t vectis-win -f Dockerfile.windows .
   ```
