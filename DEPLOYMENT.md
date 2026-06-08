# Deployment Guide

## Quick Start with Docker Compose

### Prerequisites
- Docker 20.10+
- Docker Compose 2.0+

### Steps

1. **Clone and navigate to cloud-api:**
```bash
cd gym_access_sync/cloud-api
```

2. **Configure environment:**
```bash
cp .env.example .env
# Edit .env with your secure credentials
```

3. **Start services:**
```bash
docker-compose up -d
```

4. **Verify deployment:**
```bash
docker-compose ps
curl http://localhost:8080/health
```

## Production Deployment

### Option 1: Docker Compose (Production)

1. **Update `.env` with production values:**
```bash
DB_HOST=your-db-host
DB_PORT=5432
DB_USER=gym_admin
DB_PASSWORD=STRONG_SECURE_PASSWORD
DB_NAME=gym_access
SERVER_PORT=8080
GIN_MODE=release
```

2. **Use production Docker Compose:**
```bash
docker-compose -f docker-compose.yml up -d
```

3. **Setup reverse proxy (nginx):**
```nginx
server {
    listen 443 ssl http2;
    server_name api.yourdomain.com;

    ssl_certificate /path/to/cert.pem;
    ssl_certificate_key /path/to/key.pem;

    location / {
        proxy_pass http://localhost:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

### Option 2: Kubernetes

1. **Create Kubernetes manifests:**
```yaml
# deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: gym-access-api
spec:
  replicas: 3
  selector:
    matchLabels:
      app: gym-access-api
  template:
    metadata:
      labels:
        app: gym-access-api
    spec:
      containers:
      - name: api
        image: gym-access-api:latest
        ports:
        - containerPort: 8080
        env:
        - name: DB_HOST
          valueFrom:
            secretKeyRef:
              name: db-secret
              key: host
        - name: DB_PASSWORD
          valueFrom:
            secretKeyRef:
              name: db-secret
              key: password
```

2. **Deploy:**
```bash
kubectl apply -f deployment.yaml
kubectl apply -f service.yaml
```

### Option 3: DigitalOcean (Recommended)

#### Droplet Specification
- **OS**: Ubuntu 24.04 LTS
- **CPU**: 2 vCPU
- **RAM**: 2-4 GB
- **Storage**: 50-80 GB SSD

This configuration supports:
- Go API
- PostgreSQL
- A few thousand members
- Enrollment requests
- Synchronization

#### Architecture
```
Internet
    │
    ▼
DigitalOcean Droplet
├── Nginx (SSL termination)
├── Go API (port 8080)
└── PostgreSQL (localhost only)
```

#### Initial Setup

1. **Update system:**
```bash
sudo apt update
sudo apt upgrade -y
```

2. **Install required packages:**
```bash
sudo apt install nginx postgresql git -y
```

3. **Install Go 1.21+:**
```bash
wget https://go.dev/dl/go1.21.0.linux-amd64.tar.gz
sudo tar -C /usr/local -xzf go1.21.0.linux-amd64.tar.gz
export PATH=$PATH:/usr/local/go/bin
echo 'export PATH=$PATH:/usr/local/go/bin' >> ~/.bashrc
```

#### PostgreSQL Setup

1. **Secure PostgreSQL (localhost only):**
```bash
sudo nano /etc/postgresql/16/main/postgresql.conf
# Set: listen_addresses = 'localhost'
```

2. **Create database and user:**
```bash
sudo -u postgres psql
CREATE DATABASE gym_access;
CREATE USER gym_admin WITH PASSWORD 'STRONG_SECURE_PASSWORD';
GRANT ALL PRIVILEGES ON DATABASE gym_access TO gym_admin;
\q
```

3. **Run migrations:**
```bash
cd /opt/gym-access-api
psql -U gym_admin -d gym_access -f migrations/001_init_schema.sql
```

#### Deploy Go API

1. **Clone repository:**
```bash
cd /opt
sudo git clone <your-repo-url> gym-access-api
cd gym-access-api/cloud-api
```

2. **Configure environment:**
```bash
cp .env.example .env
sudo nano .env
# Update with production values:
# DB_HOST=localhost
# DB_PORT=5432
# DB_USER=gym_admin
# DB_PASSWORD=STRONG_SECURE_PASSWORD
# DB_NAME=gym_access
# SERVER_PORT=8080
# GIN_MODE=release
```

3. **Build and setup:**
```bash
go mod download
go build -o gym-access-api
sudo mkdir -p /var/log/gym-access-api
```

4. **Create systemd service:**
```bash
sudo nano /etc/systemd/system/gym-access-api.service
```

```ini
[Unit]
Description=Gym Access API
After=network.target postgresql.service

[Service]
Type=simple
User=root
WorkingDirectory=/opt/gym-access-api/cloud-api
ExecStart=/opt/gym-access-api/cloud-api/gym-access-api
Restart=always
RestartSec=5
Environment="GIN_MODE=release"
EnvironmentFile=/opt/gym-access-api/cloud-api/.env

[Install]
WantedBy=multi-user.target
```

5. **Start service:**
```bash
sudo systemctl enable gym-access-api
sudo systemctl start gym-access-api
sudo systemctl status gym-access-api
```

#### Nginx Configuration

1. **Create nginx config:**
```bash
sudo nano /etc/nginx/sites-available/gym-access-api
```

```nginx
server {
    listen 80;
    server_name api.yourdomain.com;

    location / {
        proxy_pass http://localhost:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

2. **Enable site:**
```bash
sudo ln -s /etc/nginx/sites-available/gym-access-api /etc/nginx/sites-enabled/
sudo nginx -t
sudo systemctl restart nginx
```

#### SSL Setup with Certbot

1. **Install certbot:**
```bash
sudo apt install certbot python3-certbot-nginx -y
```

2. **Obtain SSL certificate:**
```bash
sudo certbot --nginx -d api.yourdomain.com
```

3. **Auto-renewal (certbot sets this up automatically):**
```bash
sudo certbot renew --dry-run
```

#### Firewall Configuration

```bash
sudo ufw allow 22/tcp
sudo ufw allow 80/tcp
sudo ufw allow 443/tcp
sudo ufw enable
```

#### Verify Deployment

```bash
# Check API health
curl http://localhost:8080/health
curl https://api.yourdomain.com/health

# Check nginx status
sudo systemctl status nginx

# Check API service
sudo systemctl status gym-access-api

# Check PostgreSQL
sudo systemctl status postgresql
```

### Option 4: Traditional VPS

## Database Setup

### Initial Migration
```bash
psql -U gym_admin -d gym_access -f migrations/001_init_schema.sql
```

### Create Sites
```sql
INSERT INTO sites (site_name, api_key, active) VALUES
    ('Main Gym', 'CHANGE_THIS_KEY_1', TRUE),
    ('Lekki Branch', 'CHANGE_THIS_KEY_2', TRUE),
    ('Abuja Branch', 'CHANGE_THIS_KEY_3', TRUE);
```

### Backup
```bash
pg_dump -U gym_admin gym_access > backup_$(date +%Y%m%d).sql
```

### Restore
```bash
psql -U gym_admin gym_access < backup_20240101.sql
```

## Terminal Configuration

Update `config.json` on each terminal:
```json
{
  "api_url": "https://api.yourdomain.com",
  "api_key": "CHANGE_THIS_KEY_1",
  "sync_interval_seconds": 60,
  "site_name": "Main Gym",
  "offline_mode": true,
  "scanner_ip": "192.168.1.100",
  "scanner_port": 4370,
  "enrollment_timeout": 30
}
```

## Monitoring

### Health Checks
```bash
# API health
curl https://api.yourdomain.com/health

# Database connectivity
docker exec gym-access-db pg_isready -U gym_admin
```

### Logs
```bash
# Docker logs
docker-compose logs -f api

# Systemd logs
journalctl -u gym-access-api -f
```

### Metrics (Optional)
Consider adding Prometheus metrics for:
- Request rate
- Response times
- Error rates
- Database connection pool status

## Security Checklist

- [ ] Change default API keys
- [ ] Use strong database passwords
- [ ] Enable PostgreSQL SSL
- [ ] Use HTTPS with valid certificates
- [ ] Configure firewall rules
- [ ] Set up database backups
- [ ] Enable audit logging
- [ ] Implement rate limiting
- [ ] Regular security updates
- [ ] Monitor access logs

## Troubleshooting

### API won't start
```bash
# Check database connectivity
psql -U gym_admin -d gym_access -c "SELECT 1"

# Check port availability
netstat -tulpn | grep 8080

# Check logs
docker-compose logs api
```

### Terminal can't connect
- Verify API URL is correct
- Check API key in database
- Test network connectivity
- Review terminal logs

### Database connection issues
```bash
# Check PostgreSQL status
sudo systemctl status postgresql

# Check connection
psql -U gym_admin -d gym_access -c "SELECT version()"
```

## Scaling

### Horizontal Scaling
- Deploy multiple API instances behind load balancer
- Use PostgreSQL connection pooling (PgBouncer)
- Consider read replicas for reporting queries

### Vertical Scaling
- Increase server resources
- Optimize database queries
- Add indexes for frequently queried fields

## Maintenance

### Regular Tasks
- Daily: Review access logs
- Weekly: Database backups
- Monthly: Security updates
- Quarterly: Review and rotate API keys

### Updates
```bash
# Pull latest code
git pull

# Rebuild
docker-compose build

# Restart with zero downtime
docker-compose up -d --no-deps --build api
```
