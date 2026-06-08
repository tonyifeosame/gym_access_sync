#!/bin/bash

# DigitalOcean Deployment Script for Gym Access API
# This script automates the deployment of the gym access API on a DigitalOcean droplet

set -e

# Configuration
REPO_URL="${REPO_URL:-}"
DOMAIN="${DOMAIN:-api.yourdomain.com}"
DB_PASSWORD="${DB_PASSWORD:-}"
DB_USER="${DB_USER:-gym_admin}"
DB_NAME="${DB_NAME:-gym_access}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    log_error "Please run as root (use sudo)"
    exit 1
fi

# Prompt for required values if not set
if [ -z "$REPO_URL" ]; then
    read -p "Enter your Git repository URL: " REPO_URL
fi

if [ -z "$DB_PASSWORD" ]; then
    read -s -p "Enter database password: " DB_PASSWORD
    echo
fi

if [ "$DOMAIN" = "api.yourdomain.com" ]; then
    read -p "Enter your domain (default: api.yourdomain.com): " DOMAIN
    DOMAIN=${DOMAIN:-api.yourdomain.com}
fi

log_info "Starting deployment for domain: $DOMAIN"

# Step 1: Update system
log_info "Updating system packages..."
apt update
apt upgrade -y

# Step 2: Install required packages
log_info "Installing required packages..."
apt install nginx postgresql git curl -y

# Step 3: Install Go 1.21
log_info "Installing Go 1.21..."
if [ ! -f "/usr/local/go/bin/go" ]; then
    wget https://go.dev/dl/go1.21.0.linux-amd64.tar.gz
    tar -C /usr/local -xzf go1.21.0.linux-amd64.tar.gz
    rm go1.21.0.linux-amd64.tar.gz
    echo 'export PATH=$PATH:/usr/local/go/bin' >> /root/.bashrc
    export PATH=$PATH:/usr/local/go/bin
else
    log_info "Go is already installed"
fi

# Step 4: Secure PostgreSQL
log_info "Configuring PostgreSQL..."
PG_VERSION=$(psql --version | awk '{print $3}' | cut -d. -f1,2)
PG_CONF="/etc/postgresql/${PG_VERSION}/main/postgresql.conf"

if [ -f "$PG_CONF" ]; then
    sed -i "s/#listen_addresses = 'localhost'/listen_addresses = 'localhost'/" "$PG_CONF"
    systemctl restart postgresql
else
    log_warn "Could not find PostgreSQL config at $PG_CONF"
fi

# Step 5: Create database and user
log_info "Creating database and user..."
sudo -u postgres psql -c "CREATE DATABASE ${DB_NAME};"
sudo -u postgres psql -c "CREATE USER ${DB_USER} WITH PASSWORD '${DB_PASSWORD}';"
sudo -u postgres psql -c "GRANT ALL PRIVILEGES ON DATABASE ${DB_NAME} TO ${DB_USER};"

# Step 6: Clone repository
log_info "Cloning repository..."
cd /opt
if [ -d "gym-access-api" ]; then
    log_info "Repository already exists, pulling latest changes..."
    cd gym-access-api
    git pull
else
    git clone "$REPO_URL" gym-access-api
    cd gym-access-api
fi

# Step 7: Setup Go API
log_info "Building Go API..."
cd cloud-api

# Create .env file
cat > .env << EOF
DB_HOST=localhost
DB_PORT=5432
DB_USER=${DB_USER}
DB_PASSWORD=${DB_PASSWORD}
DB_NAME=${DB_NAME}
SERVER_PORT=8080
GIN_MODE=release
EOF

# Build the application
go mod download
go build -o gym-access-api

# Create log directory
mkdir -p /var/log/gym-access-api

# Step 8: Run migrations
log_info "Running database migrations..."
if [ -f "migrations/001_init_schema.sql" ]; then
    PGPASSWORD=${DB_PASSWORD} psql -U ${DB_USER} -d ${DB_NAME} -f migrations/001_init_schema.sql
else
    log_warn "Migration file not found, skipping migrations"
fi

# Step 9: Create systemd service
log_info "Creating systemd service..."
cat > /etc/systemd/system/gym-access-api.service << EOF
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
EOF

# Step 10: Start the service
log_info "Starting gym-access-api service..."
systemctl daemon-reload
systemctl enable gym-access-api
systemctl start gym-access-api

# Step 11: Configure nginx
log_info "Configuring nginx..."
cat > /etc/nginx/sites-available/gym-access-api << EOF
server {
    listen 80;
    server_name ${DOMAIN};

    location / {
        proxy_pass http://localhost:8080;
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
        proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto \$scheme;
    }
}
EOF

# Enable nginx site
ln -sf /etc/nginx/sites-available/gym-access-api /etc/nginx/sites-enabled/
rm -f /etc/nginx/sites-enabled/default
nginx -t
systemctl restart nginx

# Step 12: Configure firewall
log_info "Configuring firewall..."
ufw allow 22/tcp
ufw allow 80/tcp
ufw allow 443/tcp
ufw --force enable

# Step 13: Verify deployment
log_info "Verifying deployment..."
sleep 3

if systemctl is-active --quiet gym-access-api; then
    log_info "✓ gym-access-api service is running"
else
    log_error "✗ gym-access-api service failed to start"
    systemctl status gym-access-api
fi

if systemctl is-active --quiet nginx; then
    log_info "✓ nginx is running"
else
    log_error "✗ nginx failed to start"
    systemctl status nginx
fi

if systemctl is-active --quiet postgresql; then
    log_info "✓ PostgreSQL is running"
else
    log_error "✗ PostgreSQL failed to start"
    systemctl status postgresql
fi

# Test API health
if curl -s http://localhost:8080/health > /dev/null; then
    log_info "✓ API health check passed"
else
    log_warn "✗ API health check failed"
fi

log_info "Deployment completed!"
log_info "Next steps:"
log_info "1. Point your domain ${DOMAIN} to this droplet's IP address"
log_info "2. Run SSL setup: bash deploy/setup-ssl.sh ${DOMAIN}"
log_info "3. Update your gym terminal config.json with: https://${DOMAIN}"
