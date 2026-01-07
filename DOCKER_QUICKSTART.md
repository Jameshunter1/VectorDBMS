# Quick Reference: Docker Commands

## After Restart, Run These Commands:

### 1. Verify Docker Installation
```powershell
# Check Docker version
docker --version

# Check Docker Compose (built into Docker Desktop)
docker compose version

# Test Docker works
docker run hello-world
```

### 2. Build Your Vectis Database Image
```powershell
# Navigate to project
cd C:\Users\James\SystemProjects\VectorDBMS

# Build image (takes 5-10 minutes first time)
docker build -t vectis:latest .
```

### 3. Start the Full Stack
```powershell
# Start database + Prometheus + Grafana
docker compose up -d

# Check all services are running
docker compose ps
```

### 4. Access Your Services
- **Database API**: http://localhost:8080
- **Prometheus Metrics**: http://localhost:9090
- **Grafana Dashboards**: http://localhost:3000 (login: admin/admin)

### 5. View Logs
```powershell
# All services
docker compose logs -f

# Just database
docker compose logs -f vectis

# Just Prometheus
docker compose logs -f prometheus
```

### 6. Stop Services
```powershell
# Stop but keep data
docker compose down

# Stop and delete all data
docker compose down -v
```

## VS Code Docker Sidebar

After restart, click the **Docker icon** (whale) in VS Code sidebar to see:
- **Containers** (your running services)
- **Images** (vectis:latest after build)
- **Registries** (connect to Docker Hub)
- **Volumes** (persistent data)

### Quick Actions (Right-Click):
- **Dockerfile** → "Build Image" (builds vectis:latest)
- **docker-compose.yml** → "Compose Up" (starts all services)
- **Container** → "View Logs", "Attach Shell", "Stop"
- **Image** → "Run", "Push to Registry", "Inspect"

## Common Issues After Restart

### Docker Desktop not starting?
1. Open "Docker Desktop" app manually from Start Menu
2. Wait for whale icon in system tray to stop animating
3. Try commands again

### Port 8080 already in use?
```powershell
# Find what's using it
netstat -ano | findstr :8080

# Or change port in docker-compose.yml
# Change "8080:8080" to "8081:8080"
```

### Want to rebuild after code changes?
```powershell
# Rebuild image
docker build -t vectis:latest .

# Restart containers with new image
docker compose down
docker compose up -d
```

---

**See full guides:**
- [DOCKER_SETUP.md](DOCKER_SETUP.md) - Complete setup instructions
- [DOCKER_GUIDE.md](DOCKER_GUIDE.md) - Production deployment guide
