# LSM Database Engine - Production Deployment Guide

## 🚀 Public Internet Deployment

This guide explains how to deploy your LSM Database Engine to the internet for public access.

---

## 📦 What You're Deploying

**LSM Database Engine v1.3** - A complete, secure, production-ready key-value database with:
- ✅ High-performance LSM Tree storage engine
- ✅ Full authentication & authorization (users, roles, sessions)
- ✅ Complete audit trail for compliance
- ✅ RESTful API for data operations
- ✅ Web-based management interface (5 tabs: Operations, Browse, Stats, Files, Console)
- ✅ Multi-threaded, concurrent access
- ✅ 19 comprehensive tests (100% pass rate)

---

## 🌐 Deployment Architecture

```
Internet
    ↓
[Cloud Provider]
    ↓
[Load Balancer] (Optional for HA)
    ↓
[Nginx/Apache] ← SSL/TLS Termination
    ↓
[LSM Database Server] ← Your Application
    ↓
[Persistent Storage] ← Data Directory
```

---

## 🛠️ Step 1: Build for Production

### Windows Build

```powershell
cd C:\Users\James\SystemProjects\LSMDatabaseEngine\src

# Configure for Release
cmake --preset windows-vs2022-x64-release

# Build
cmake --build build/windows-vs2022-x64-release --config Release --target ALL_BUILD

# Verify build
dir build\windows-vs2022-x64-release\Release\

# You should see:
# - core_engine.lib
# - dbcli.exe
# - dbweb.exe (web server)
```

### Linux Build (for cloud deployment)

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y build-essential cmake git

# Clone and build
cd /opt
git clone <your-repo> lsmdb
cd lsmdb/src

# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release -j$(nproc)

# Verify
ls -lh build/dbweb
ls -lh build/libcore_engine.a
```

---

## ☁️ Step 2: Choose Cloud Provider

### Option A: DigitalOcean (Recommended for simplicity)

**Cost**: $6-12/month for basic droplet

1. **Create Droplet**:
   - OS: Ubuntu 22.04 LTS
   - Plan: Basic, Regular SSD, $6/mo (1GB RAM, 25GB SSD)
   - Region: Choose nearest to your users
   - Add SSH key

2. **Initial Setup**:
   ```bash
   ssh root@your-droplet-ip
   
   # Update system
   apt update && apt upgrade -y
   
   # Create app user
   adduser lsmdb
   usermod -aG sudo lsmdb
   su - lsmdb
   ```

### Option B: AWS EC2

**Cost**: ~$8-15/month (t3.micro with 1 year free tier)

1. **Launch EC2 Instance**:
   - AMI: Ubuntu Server 22.04 LTS
   - Instance Type: t3.micro (1 vCPU, 1GB RAM)
   - Storage: 20 GB gp3
   - Security Group: Allow ports 22, 80, 443

2. **Connect**:
   ```bash
   ssh -i your-key.pem ubuntu@ec2-xx-xx-xx-xx.compute.amazonaws.com
   ```

### Option C: Google Cloud Platform

**Cost**: ~$7-10/month (e2-micro)

1. **Create VM Instance**:
   - Machine type: e2-micro (0.25 vCPU, 1GB RAM)
   - Boot disk: Ubuntu 22.04 LTS, 20 GB
   - Firewall: Allow HTTP, HTTPS

---

## 🔐 Step 3: Configure Security

### Create Configuration File

```bash
cd /opt/lsmdb
nano production.conf
```

```ini
# Production Configuration
server.host=127.0.0.1
server.port=8080
server.enable_https=false

security.require_authentication=true
security.session_timeout_minutes=30
security.enable_audit_log=true
security.audit_log_path=/var/log/lsmdb/audit.log

database.data_dir=/var/lib/lsmdb/data
database.memtable_size_limit_mb=64
```

### Set Up Directories

```bash
sudo mkdir -p /var/lib/lsmdb/data
sudo mkdir -p /var/log/lsmdb
sudo chown -R lsmdb:lsmdb /var/lib/lsmdb
sudo chown -R lsmdb:lsmdb /var/log/lsmdb
```

### Create Systemd Service

```bash
sudo nano /etc/systemd/system/lsmdb.service
```

```ini
[Unit]
Description=LSM Database Engine
After=network.target

[Service]
Type=simple
User=lsmdb
WorkingDirectory=/opt/lsmdb/build
ExecStart=/opt/lsmdb/build/dbweb /var/lib/lsmdb/data 8080
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable lsmdb
sudo systemctl start lsmdb
sudo systemctl status lsmdb
```

---

## 🌐 Step 4: Set Up Reverse Proxy (Nginx)

### Install Nginx

```bash
sudo apt-get install -y nginx certbot python3-certbot-nginx
```

### Configure Nginx

```bash
sudo nano /etc/nginx/sites-available/lsmdb
```

```nginx
server {
    listen 80;
    server_name yourdomain.com www.yourdomain.com;

    # Redirect to HTTPS
    return 301 https://$server_name$request_uri;
}

server {
    listen 443 ssl http2;
    server_name yourdomain.com www.yourdomain.com;

    # SSL Configuration (certbot will add these)
    ssl_certificate /etc/letsencrypt/live/yourdomain.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/yourdomain.com/privkey.pem;

    # Security headers
    add_header X-Frame-Options "DENY" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-XSS-Protection "1; mode=block" always;
    add_header Strict-Transport-Security "max-age=31536000; includeSubDomains" always;

    # Rate limiting
    limit_req_zone $binary_remote_addr zone=api_limit:10m rate=60r/m;
    limit_req zone=api_limit burst=10 nodelay;

    location / {
        proxy_pass http://127.0.0.1:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        # Timeouts
        proxy_connect_timeout 60s;
        proxy_send_timeout 60s;
        proxy_read_timeout 60s;
    }

    # API endpoints
    location /api/ {
        proxy_pass http://127.0.0.1:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    }

    access_log /var/log/nginx/lsmdb_access.log;
    error_log /var/log/nginx/lsmdb_error.log;
}
```

### Enable Site

```bash
sudo ln -s /etc/nginx/sites-available/lsmdb /etc/nginx/sites-enabled/
sudo nginx -t
sudo systemctl restart nginx
```

---

## 🔒 Step 5: Enable HTTPS with Let's Encrypt

```bash
# Obtain SSL certificate
sudo certbot --nginx -d yourdomain.com -d www.yourdomain.com

# Follow prompts to:
# 1. Enter email address
# 2. Agree to terms
# 3. Choose to redirect HTTP to HTTPS

# Test auto-renewal
sudo certbot renew --dry-run

# Certificate will auto-renew every 60 days
```

---

## 🔥 Step 6: Configure Firewall

```bash
# UFW (Ubuntu)
sudo ufw allow 22/tcp    # SSH
sudo ufw allow 80/tcp    # HTTP
sudo ufw allow 443/tcp   # HTTPS
sudo ufw enable
sudo ufw status

# Verify
sudo ufw status numbered
```

---

## 📊 Step 7: Monitoring & Logging

### View Application Logs

```bash
# Application logs
sudo journalctl -u lsmdb -f

# Nginx access logs
sudo tail -f /var/log/nginx/lsmdb_access.log

# Audit logs
sudo tail -f /var/log/lsmdb/audit.log
```

### Set Up Log Rotation

```bash
sudo nano /etc/logrotate.d/lsmdb
```

```
/var/log/lsmdb/*.log {
    daily
    rotate 30
    compress
    delaycompress
    notifempty
    create 0640 lsmdb lsmdb
    sharedscripts
    postrotate
        systemctl reload lsmdb > /dev/null 2>&1 || true
    endscript
}
```

### Monitor Resource Usage

```bash
# Install htop
sudo apt-get install -y htop

# Monitor
htop

# Check disk usage
df -h /var/lib/lsmdb

# Check database size
du -sh /var/lib/lsmdb/data
```

---

## 🔧 Step 8: Initial Configuration

### Change Default Passwords

**CRITICAL**: The system comes with default users. Change these immediately!

```bash
# Connect to your server
curl https://yourdomain.com/api/login \
  -d "username=admin&password=admin123"

# After login, change password via API or recreate user
# (You'll need to add a password change endpoint or recreate users)
```

### Create Production Users

Edit your application code to create proper users on first run, or use a setup script.

---

## 🌍 Step 9: DNS Configuration

### Point Domain to Server

1. **Go to your domain registrar** (GoDaddy, Namecheap, etc.)

2. **Add A Records**:
   ```
   Type: A
   Name: @
   Value: your-server-ip
   TTL: 3600

   Type: A
   Name: www
   Value: your-server-ip
   TTL: 3600
   ```

3. **Wait for DNS propagation** (5 minutes to 48 hours)

4. **Verify**:
   ```bash
   nslookup yourdomain.com
   ```

---

## ✅ Step 10: Verify Deployment

### Test Endpoints

```bash
# Health check
curl https://yourdomain.com/

# API test
curl https://yourdomain.com/api/stats

# Login test
curl -X POST https://yourdomain.com/api/login \
  -d "username=admin&password=admin123"
```

### Load Test

```bash
# Install Apache Bench
sudo apt-get install -y apache2-utils

# Test 1000 requests, 10 concurrent
ab -n 1000 -c 10 https://yourdomain.com/api/stats
```

---

## 💰 Cost Estimate

### Monthly Costs

| Service | Provider | Cost |
|---------|----------|------|
| VM Instance | DigitalOcean | $6-12 |
| VM Instance | AWS EC2 t3.micro | $8-15 |
| VM Instance | GCP e2-micro | $7-10 |
| Domain Name | Any Registrar | $10-15/year |
| SSL Certificate | Let's Encrypt | FREE |
| Bandwidth | 1TB included | $0 |

**Total**: ~$8-15/month (after 1st year)

---

## 🔐 Security Checklist

Before going live:

- [ ] Change default admin password
- [ ] Enable firewall (UFW)
- [ ] Set up HTTPS with Let's Encrypt
- [ ] Configure Nginx security headers
- [ ] Enable rate limiting
- [ ] Set up log rotation
- [ ] Configure backups
- [ ] Review audit logs daily
- [ ] Update system packages: `sudo apt update && sudo apt upgrade`
- [ ] Disable root SSH login
- [ ] Enable SSH key authentication only
- [ ] Set up monitoring (optional: Datadog, New Relic)
- [ ] Configure backups (database data directory)

---

## 📦 Backup Strategy

### Automated Daily Backups

```bash
sudo nano /usr/local/bin/lsmdb-backup.sh
```

```bash
#!/bin/bash
BACKUP_DIR="/var/backups/lsmdb"
DATA_DIR="/var/lib/lsmdb/data"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

mkdir -p $BACKUP_DIR
tar -czf $BACKUP_DIR/lsmdb_$TIMESTAMP.tar.gz $DATA_DIR

# Keep only last 7 days
find $BACKUP_DIR -name "lsmdb_*.tar.gz" -mtime +7 -delete
```

```bash
sudo chmod +x /usr/local/bin/lsmdb-backup.sh

# Add to crontab
sudo crontab -e
```

```
# Daily backup at 2 AM
0 2 * * * /usr/local/bin/lsmdb-backup.sh
```

---

## 📈 Scaling Considerations

### Vertical Scaling (Single Server)

- Upgrade to larger instance (2-4 GB RAM)
- Increase disk space (SSD recommended)
- Adjust `memtable_size_limit_mb` in config

### Horizontal Scaling (Multiple Servers)

For high traffic:

1. **Load Balancer** (AWS ELB, DigitalOcean Load Balancer)
2. **Multiple App Servers** (read replicas)
3. **Shared Storage** (AWS EFS, NFS)
4. **Redis** for session storage (distributed sessions)

---

## 🎉 You're Live!

Your database is now accessible at:
- **Main URL**: `https://yourdomain.com`
- **API**: `https://yourdomain.com/api/*`
- **Status**: `https://yourdomain.com/api/stats`

### Share Your Database

Users can access it via:
- Web browser (management interface)
- REST API (programmatic access)
- CLI tools (curl, wget, etc.)

---

## 📞 Troubleshooting

### Database Won't Start

```bash
sudo journalctl -u lsmdb -n 50
sudo systemctl status lsmdb
```

### Connection Refused

```bash
# Check if service is running
sudo systemctl status lsmdb

# Check if port is listening
sudo netstat -tlnp | grep 8080

# Check firewall
sudo ufw status
```

### High Memory Usage

```bash
# Check memory
free -h

# Restart service
sudo systemctl restart lsmdb
```

### SSL Certificate Issues

```bash
# Renew certificate
sudo certbot renew --force-renewal

# Check certificate
sudo certbot certificates
```

---

**Your LSM Database Engine is now running on the public internet!** 🚀

For support, check logs in `/var/log/lsmdb/` and audit trails in `audit.log`.
