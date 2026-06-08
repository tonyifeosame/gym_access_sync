#!/bin/bash

# SSL Setup Script for DigitalOcean Deployment
# This script installs certbot and obtains SSL certificates for the gym access API

set -e

DOMAIN=$1

if [ -z "$DOMAIN" ]; then
    echo "Usage: $0 <domain>"
    echo "Example: $0 api.yourdomain.com"
    exit 1
fi

echo "Setting up SSL for domain: $DOMAIN"

# Update package list
sudo apt update

# Install certbot and nginx plugin
sudo apt install certbot python3-certbot-nginx -y

# Obtain SSL certificate
echo "Obtaining SSL certificate for $DOMAIN..."
sudo certbot --nginx -d "$DOMAIN"

# Test auto-renewal
echo "Testing SSL certificate auto-renewal..."
sudo certbot renew --dry-run

echo "SSL setup complete!"
echo "Your API is now available at: https://$DOMAIN"
echo "Certificates will auto-renew automatically."
