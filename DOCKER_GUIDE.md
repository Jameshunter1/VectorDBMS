# Docker Deployment Guide for Vectis Database

## Quick Start (Production)

```bash
# Start database + monitoring stack
docker compose up -d

# Check status
docker compose ps

# View logs
docker compose logs -f vectis

# Stop everything
docker compose down
```

**Access Points:**
- **Database API**: http://localhost:8080
- **Prometheus**: http://localhost:9090
- **Grafana**: http://localhost:3000 (admin/admin)

## Development Mode

```bash
# Build image
docker build -t vectis:dev .

# Run database only (no monitoring)
docker run -d \
  -p 8080:8080 \
  -v vectis-data:/vectis/data \
  --name vectis \
  vectis:dev

# Access interactive shell
docker exec -it vectis /bin/bash

# Run CLI inside container
docker exec -it vectis ./dbcli /vectis/data
```

## Production Deployment

### AWS ECS

```bash
# Build and push to ECR
aws ecr get-login-password --region us-east-1 | docker login --username AWS --password-stdin <account>.dkr.ecr.us-east-1.amazonaws.com
docker build -t vectis:latest .
docker tag vectis:latest <account>.dkr.ecr.us-east-1.amazonaws.com/vectis:latest
docker push <account>.dkr.ecr.us-east-1.amazonaws.com/vectis:latest

# Create ECS task definition (use docker-compose.yml as reference)
```

### Docker Swarm

```bash
# Initialize swarm
docker swarm init

# Deploy stack
docker stack deploy -c docker-compose.yml vectis

# Scale database
docker service scale vectis_vectis=3

# Check services
docker service ls
docker service logs vectis_vectis
```

### Kubernetes

```bash
# Convert docker-compose to k8s manifests
kompose convert -f docker-compose.yml

# Apply to cluster
kubectl apply -f .

# Expose service
kubectl expose deployment vectis --type=LoadBalancer --port=8080
```

## Configuration

### Environment Variables

```yaml
# docker-compose.yml
environment:
  - VECTIS_DATA_DIR=/vectis/data
  - VECTIS_PORT=8080
  - VECTIS_HOST=0.0.0.0
  - VECTIS_BUFFER_POOL_SIZE_MB=256      # Buffer pool size
  - VECTIS_ENABLE_VECTOR_INDEX=true     # Enable HNSW indexing
  - VECTIS_VECTOR_DIMENSION=384         # Embedding dimension
  - VECTIS_LOG_LEVEL=INFO               # DEBUG, INFO, WARN, ERROR
```

### Volume Management

```bash
# List volumes
docker volume ls

# Inspect volume
docker volume inspect vectis-data

# Backup data
docker run --rm \
  -v vectis-data:/data \
  -v $(pwd):/backup \
  ubuntu tar czf /backup/vectis-backup.tar.gz /data

# Restore data
docker run --rm \
  -v vectis-data:/data \
  -v $(pwd):/backup \
  ubuntu tar xzf /backup/vectis-backup.tar.gz -C /
```

## Monitoring

### Prometheus Metrics

```bash
# Scrape metrics manually
curl http://localhost:8080/metrics

# Query Prometheus
open http://localhost:9090/graph
# Example query: rate(core_engine_requests_total[5m])
```

### Grafana Dashboards

1. Open http://localhost:3000
2. Login: admin/admin
3. Add Prometheus data source: http://prometheus:9090
4. Import dashboard from `monitoring/grafana-dashboards/`

## Health Checks

```bash
# Check health endpoint
curl http://localhost:8080/api/health

# Expected response:
# {"status":"healthy","database_open":true,"wal_healthy":true}

# Docker health status
docker inspect --format='{{.State.Health.Status}}' vectis
```

## Troubleshooting

### Container won't start

```bash
# Check logs
docker compose logs vectis

# Common issues:
# 1. Port 8080 already in use → Change in docker-compose.yml
# 2. Insufficient memory → Adjust resource limits
# 3. Volume permission issues → Run: docker compose down -v
```

### High memory usage

```bash
# Check resource usage
docker stats vectis

# Adjust buffer pool size
docker compose down
# Edit docker-compose.yml: VECTIS_BUFFER_POOL_SIZE_MB=128
docker compose up -d
```

### Slow queries

```bash
# Check metrics
curl http://localhost:8080/api/stats

# Enable debug logging
docker compose down
# Add: VECTIS_LOG_LEVEL=DEBUG
docker compose up -d
docker compose logs -f vectis
```

## Security

### Production Hardening

1. **Use secrets for sensitive config**
   ```yaml
   secrets:
     - vectis_admin_password
   ```

2. **Enable TLS/HTTPS**
   - Use reverse proxy (nginx, Traefik)
   - Mount certificates into container

3. **Network isolation**
   ```yaml
   networks:
     - internal  # Database not exposed publicly
   ```

4. **Regular updates**
   ```bash
   docker compose pull
   docker compose up -d
   ```

## Performance Tuning

### CPU Optimization

```yaml
deploy:
  resources:
    limits:
      cpus: '4.0'  # Increase for high throughput
    reservations:
      cpus: '2.0'
```

### Memory Optimization

```yaml
environment:
  - VECTIS_BUFFER_POOL_SIZE_MB=1024  # Increase cache size
```

### I/O Optimization

```bash
# Use SSD-backed volumes
docker volume create --driver local \
  --opt type=none \
  --opt device=/mnt/fast-ssd/vectis \
  --opt o=bind \
  vectis-data
```

## Multi-Instance Setup

```yaml
# docker-compose.yml
services:
  vectis-1:
    image: vectis:latest
    ports:
      - "8081:8080"
    volumes:
      - vectis-data-1:/vectis/data
  
  vectis-2:
    image: vectis:latest
    ports:
      - "8082:8080"
    volumes:
      - vectis-data-2:/vectis/data
  
  nginx:
    image: nginx
    ports:
      - "80:80"
    # Load balancer configuration
```

## CI/CD Integration

### GitHub Actions

```yaml
# .github/workflows/docker.yml
- name: Build Docker image
  run: docker build -t vectis:${{ github.sha }} .

- name: Run tests in container
  run: |
    docker run --rm vectis:${{ github.sha }} ./core_engine_tests

- name: Push to registry
  run: docker push myregistry/vectis:${{ github.sha }}
```

## Support

- **Documentation**: See [README.md](README.md)
- **Issues**: https://github.com/Jameshunter1/VectorDBMS/issues
- **Docker Hub**: (Coming soon)
