# Docker Setup Guide for Windows

## ✅ Installation Complete

You've successfully installed:
- ✅ **Docker Desktop 4.55.0**
- ✅ **VS Code Docker Extension**
- ✅ **VS Code Dev Containers Extension**

## 🔄 Next Steps (REQUIRED)

### 1. Restart Your Computer
**IMPORTANT:** Docker requires a system restart to complete installation.

```powershell
# Save your work, then restart
Restart-Computer
```

### 2. Start Docker Desktop
After restart:
1. Press `Win` key and search for "Docker Desktop"
2. Launch Docker Desktop
3. Wait for Docker engine to start (whale icon in system tray will stop animating)
4. Accept the Docker Subscription Service Agreement if prompted

### 3. Verify Installation

Open PowerShell and run:

```powershell
# Check Docker version
docker --version
# Expected: Docker version 24.0.x, build xxxxx

# Check Docker Compose (built into Docker Desktop)
docker compose version
# Expected: Docker Compose version v2.x.x

# Test Docker is working
docker run hello-world
# Expected: "Hello from Docker!" message
```

## 🐳 Using Docker in VS Code

### View Docker Sidebar

1. Click the **Docker icon** in VS Code Activity Bar (left sidebar)
2. You'll see sections for:
   - **Containers** (running/stopped containers)
   - **Images** (downloaded images)
   - **Registries** (Docker Hub, Azure Container Registry, etc.)
   - **Networks** (container networks)
   - **Volumes** (persistent data)

### Quick Actions from VS Code

**Right-click on:**
- **Image** → Run, Inspect, Push to registry, Remove
- **Container** → View logs, Attach shell, Start/Stop/Restart, Remove
- **Dockerfile** → Build Image, Build & Run

### Build Your Vectis Image

```powershell
# In VS Code terminal (PowerShell), navigate to project root
cd C:\Users\James\SystemProjects\VectorDBMS

# Build the image
docker build -t vectis:latest .

# Or use VS Code: Right-click Dockerfile → "Build Image..."
```

### Start the Full Stack

```powershell
# Start Vectis + Prometheus + Grafana
docker compose up -d

# View in VS Code Docker sidebar: Containers section will show:
# - vectis_vectis_1 (database)
# - vectis_prometheus_1 (metrics)
# - vectis_grafana_1 (dashboards)
```

## 🔍 VS Code Docker Features

### 1. Dockerfile IntelliSense
- Auto-completion for commands (`FROM`, `RUN`, `COPY`)
- Hover documentation
- Syntax highlighting

### 2. Docker Compose IntelliSense
- YAML auto-completion
- Service validation
- Hover documentation

### 3. Container Management
**From VS Code:**
- View logs: Right-click container → "View Logs"
- Attach shell: Right-click container → "Attach Shell"
- Inspect: Right-click container → "Inspect"
- Port forwarding: Automatic when container starts

### 4. Image Management
**From VS Code:**
- Pull images: Click "+" next to Images → Search Docker Hub
- Build images: Right-click Dockerfile → "Build Image"
- Push to registry: Right-click image → "Push"

### 5. Registry Management
**Add registries:**
1. Click "+" next to Registries in Docker sidebar
2. Choose:
   - **Docker Hub** (requires account)
   - **Azure Container Registry**
   - **GitHub Container Registry**
   - **Generic Registry** (private registry)

## 📦 Connecting to Docker Hub

### Create Docker Hub Account
1. Go to https://hub.docker.com/signup
2. Create free account
3. Verify email

### Login from VS Code

**Option 1: VS Code UI**
1. Docker sidebar → Registries → Click "+"
2. Select "Docker Hub"
3. Enter username and password

**Option 2: Terminal**
```powershell
docker login
# Username: your-dockerhub-username
# Password: your-dockerhub-password
```

### Push Your Image to Docker Hub

```powershell
# Tag your image with Docker Hub username
docker tag vectis:latest yourusername/vectis:latest

# Push to Docker Hub
docker push yourusername/vectis:latest

# Or from VS Code: Right-click image → "Push" → Select registry
```

## 🚀 Quick Start Commands

### Development Workflow

```powershell
# 1. Build image
docker build -t vectis:dev .

# 2. Run database only (no monitoring)
docker run -d -p 8080:8080 --name vectis-dev vectis:dev

# 3. Check logs
docker logs -f vectis-dev

# 4. Access database
Start-Process "http://localhost:8080"

# 5. Stop and remove
docker stop vectis-dev
docker rm vectis-dev
```

### Production Workflow

```powershell
# 1. Start full stack (database + monitoring)
docker compose up -d

# 2. Check all services
docker compose ps

# 3. View logs for specific service
docker compose logs -f vectis

# 4. Access services
Start-Process "http://localhost:8080"      # Database API
Start-Process "http://localhost:9090"      # Prometheus
Start-Process "http://localhost:3000"      # Grafana (admin/admin)

# 5. Stop all services
docker compose down

# 6. Stop and remove volumes (delete data)
docker compose down -v
```

## 🛠️ Troubleshooting

### Issue: "Docker daemon is not running"

**Solution:**
1. Open Docker Desktop app
2. Wait for whale icon to stop animating
3. Check Settings → Resources → WSL Integration (if using WSL)

### Issue: "Cannot connect to Docker daemon"

**Solution:**
```powershell
# Restart Docker Desktop
Stop-Process -Name "Docker Desktop" -Force
Start-Process "C:\Program Files\Docker\Docker\Docker Desktop.exe"
```

### Issue: "Port already in use"

**Solution:**
```powershell
# Find what's using port 8080
netstat -ano | findstr :8080

# Kill the process (use PID from above)
taskkill /PID <PID> /F

# Or change port in docker-compose.yml:
# ports:
#   - "8081:8080"  # Change 8080 to 8081
```

### Issue: Build fails with "executable file not found"

**Solution:**
This is already fixed in commit `d54be3d`. Make sure you have latest Dockerfile:
```dockerfile
COPY --from=builder /build/build/Release/dbweb* ./dbweb
COPY --from=builder /build/build/Release/dbcli* ./dbcli
```

### Issue: "docker-compose: command not found"

**Solution:**
Docker Desktop includes Docker Compose as a plugin. Use:
```powershell
# NEW syntax (Docker Compose V2)
docker compose up -d

# OLD syntax (deprecated)
docker-compose up -d
```

## 🎯 VS Code Keyboard Shortcuts

| Action | Shortcut |
|--------|----------|
| Open Docker sidebar | `Ctrl+Shift+P` → "Docker: Focus on..." |
| Build image | `Ctrl+Shift+P` → "Docker: Build Image" |
| Run container | `Ctrl+Shift+P` → "Docker: Run" |
| View container logs | `Ctrl+Shift+P` → "Docker: Show Logs" |
| Attach shell | `Ctrl+Shift+P` → "Docker: Attach Shell" |

## 📚 Useful Resources

- **Docker Desktop Docs**: https://docs.docker.com/desktop/windows/
- **VS Code Docker Extension**: https://marketplace.visualstudio.com/items?itemName=ms-azuretools.vscode-docker
- **Docker Compose Reference**: https://docs.docker.com/compose/compose-file/
- **Dockerfile Best Practices**: https://docs.docker.com/develop/develop-images/dockerfile_best-practices/

## 🔐 Security Best Practices

### 1. Never Commit Secrets
Add to `.gitignore`:
```
.env
docker-compose.override.yml
*.key
*.pem
```

### 2. Use Docker Secrets (Production)
```yaml
# docker-compose.yml
services:
  vectis:
    secrets:
      - db_password
secrets:
  db_password:
    file: ./secrets/db_password.txt
```

### 3. Scan Images for Vulnerabilities
```powershell
# Scan your image
docker scan vectis:latest

# Or use Trivy (already in CI/CD)
docker run --rm -v /var/run/docker.sock:/var/run/docker.sock aquasec/trivy image vectis:latest
```

### 4. Run as Non-Root (Already Implemented)
Your Dockerfile already includes:
```dockerfile
USER vectis:vectis  # Non-root user ✅
```

## ✨ Next Steps After Restart

1. ✅ **Restart computer**
2. ✅ **Launch Docker Desktop**
3. ✅ **Run `docker --version`** to verify
4. ✅ **Build your Vectis image**: `docker build -t vectis:latest .`
5. ✅ **Start the stack**: `docker compose up -d`
6. ✅ **Access dashboard**: http://localhost:8080
7. ✅ **View Grafana**: http://localhost:3000 (admin/admin)

## 🎉 You're Ready to Docker!

After restart, open VS Code and:
1. Click Docker icon in sidebar
2. Right-click `Dockerfile` → "Build Image"
3. Watch your Vectis database build in the terminal
4. Right-click `docker-compose.yml` → "Compose Up"
5. Access your database at http://localhost:8080

**Welcome to containerized development! 🐳**
