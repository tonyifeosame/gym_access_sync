# Gym Access System - Cloud Architecture

## Overview

This document describes the complete cloud architecture for the Gym Access System, including the Go API backend, PostgreSQL database, and integration with the C++ terminal.

## System Architecture

```
┌─────────────────────┐
│     Admin Portal    │
│ (Web Dashboard)     │
└──────────┬──────────┘
           │ HTTPS
           ▼
┌─────────────────────┐
│      Go API         │
│  /api/v1/*          │
│  Authentication     │
│  Members            │
│  Enrollment         │
│  Access Logs        │
│  Sync Engine        │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│     PostgreSQL      │
│  - members          │
│  - sites            │
│  - enrollment_reqs  │
│  - access_logs      │
└──────────┬──────────┘
           │
           │ HTTPS + X-API-Key
           ▼
┌─────────────────────┐
│ Gym Access Terminal │
│     (C++)           │
│ SQLite Cache        │
│ Offline Mode        │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│ Fingerprint Reader  │
│ (ZKTeco)            │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│ Relay + Maglock     │
└─────────────────────┘
```

## Components

### 1. Go API Backend (`cloud-api/`)

**Technology Stack:**
- Go 1.21+
- Gin web framework
- PostgreSQL (lib/pq driver)
- JWT (for future token-based auth)

**Key Features:**
- RESTful API with `/api/v1/` prefix
- API key authentication via `X-API-Key` header
- CORS support
- Health check endpoint
- Structured logging

**Directory Structure:**
```
cloud-api/
├── main.go                 # Application entry point
├── go.mod                  # Go dependencies
├── .env.example            # Environment variables template
├── Dockerfile              # Docker build configuration
├── docker-compose.yml      # Docker orchestration
├── models/                 # Data models
│   └── models.go
├── database/               # Database layer
│   ├── db.go              # Connection management
│   └── queries.go         # SQL queries
├── handlers/               # HTTP handlers
│   ├── members.go         # Member endpoints
│   ├── access.go          # Access control endpoints
│   └── enrollment.go      # Enrollment endpoints
├── middleware/             # HTTP middleware
│   └── auth.go            # Authentication & CORS
└── migrations/             # Database migrations
    └── 001_init_schema.sql
```

### 2. PostgreSQL Database

**Tables:**

#### `sites`
Stores gym locations and their API keys for authentication.
```sql
- id (SERIAL PRIMARY KEY)
- site_name (VARCHAR(100) UNIQUE)
- api_key (VARCHAR(255) UNIQUE)
- active (BOOLEAN)
- created_at (TIMESTAMP)
- updated_at (TIMESTAMP)
```

#### `members`
Stores member information and fingerprint templates.
```sql
- id (SERIAL PRIMARY KEY)
- member_id (VARCHAR(50) UNIQUE)
- full_name (TEXT)
- membership_type (TEXT)
- active (BOOLEAN)
- fingerprint_template (TEXT)
- created_at (TIMESTAMP)
- updated_at (TIMESTAMP)
```

#### `enrollment_requests`
Tracks fingerprint enrollment workflow.
```sql
- id (SERIAL PRIMARY KEY)
- member_id (VARCHAR(50))
- status (VARCHAR(20)) -- PENDING, IN_PROGRESS, COMPLETED, FAILED
- created_at (TIMESTAMP)
- completed_at (TIMESTAMP)
```

#### `access_logs`
Records all access attempts.
```sql
- id (SERIAL PRIMARY KEY)
- member_id (VARCHAR(50))
- granted (BOOLEAN)
- source (VARCHAR(50))
- site_name (VARCHAR(100))
- message (TEXT)
- created_at (TIMESTAMP)
```

### 3. C++ Terminal Integration

**Updated Files:**
- `api_client.cpp` - Updated to use `/api/v1/` endpoints and new JSON format

**Key Changes:**
- All API calls now use `/api/v1/` prefix
- JSON parsing updated to match Go API response format:
  - `full_name` instead of `member_name`
  - `fingerprint_template` instead of `member_fingerprint_template`
  - `active` (boolean) mapped to `member_status` string
  - `membership_type` instead of `member_expiring_date`
  - `updated_at` instead of `last_updated`

**Offline Mode:**
- Terminal maintains SQLite cache
- Falls back to local cache when cloud is unavailable
- Syncs changes when connection restored

## API Endpoints

### Authentication
All endpoints (except `/health`) require `X-API-Key` header.

### Members
- `GET /api/v1/members` - Get all members
- `GET /api/v1/members/:id` - Get member by ID
- `POST /api/v1/members` - Create new member
- `PUT /api/v1/members/:id` - Update member
- `DELETE /api/v1/members/:id` - Delete member
- `GET /api/v1/members/changes?since=timestamp` - Get changed members (sync)

### Access
- `GET /api/v1/access/:member_id` - Check member access
- `POST /api/v1/access/log` - Log access attempt
- `GET /api/v1/access/logs` - Get access logs
- `GET /api/v1/access/logs/:member_id` - Get member's access logs

### Enrollment
- `POST /api/v1/enrollment/start` - Start enrollment request
- `GET /api/v1/enrollment/pending` - Get pending enrollments
- `POST /api/v1/enrollment/result` - Submit fingerprint template

### Health
- `GET /health` - Health check (no auth required)

## Data Flow

### Enrollment Flow
```
Admin Portal → POST /api/v1/members → Create member
                    ↓
              POST /api/v1/enrollment/start → Create enrollment request
                    ↓
Terminal → GET /api/v1/enrollment/pending → Poll for pending requests
                    ↓
ZKTeco → Capture fingerprint → Get template
                    ↓
Terminal → POST /api/v1/enrollment/result → Submit template
                    ↓
Cloud → Update member fingerprint → Mark enrollment complete
```

### Access Flow
```
Member places finger → ZKTeco identifies member
                    ↓
Terminal → GET /api/v1/access/:member_id → Check access
                    ↓
Cloud → Check member.active → Return granted/denied
                    ↓
Terminal → If granted → Activate relay → Unlock door
                    ↓
Terminal → POST /api/v1/access/log → Log attempt
```

### Sync Flow
```
Terminal → GET /api/v1/members/changes?since=timestamp
                    ↓
Cloud → Return members changed since last sync
                    ↓
Terminal → Update SQLite cache
                    ↓
Terminal → Record last sync time
```

## Deployment

### Using Docker Compose (Recommended)

```bash
cd cloud-api
cp .env.example .env
# Edit .env with your credentials
docker-compose up -d
```

### Manual Deployment

1. **Setup PostgreSQL:**
```bash
createdb gym_access
psql gym_access < migrations/001_init_schema.sql
```

2. **Configure Environment:**
```bash
export DB_HOST=localhost
export DB_PORT=5432
export DB_USER=gym_admin
export DB_PASSWORD=your_password
export DB_NAME=gym_access
export SERVER_PORT=8080
```

3. **Run API:**
```bash
go run main.go
```

## Security Considerations

1. **API Keys:** Change default API keys in production
2. **Database:** Use strong passwords and enable SSL
3. **HTTPS:** Use HTTPS in production (configure reverse proxy)
4. **Rate Limiting:** Implement rate limiting for API endpoints
5. **Input Validation:** All inputs are validated via Gin binding
6. **SQL Injection:** Parameterized queries prevent SQL injection

## Configuration

### Terminal Config (`config.json`)
```json
{
  "api_url": "http://your-cloud-api:8080",
  "api_key": "your-site-api-key",
  "sync_interval_seconds": 60,
  "site_name": "Main Gym",
  "offline_mode": true,
  "scanner_ip": "192.168.1.100",
  "scanner_port": 4370,
  "enrollment_timeout": 30
}
```

### Cloud API Environment Variables
```bash
DB_HOST=localhost
DB_PORT=5432
DB_USER=gym_admin
DB_PASSWORD=secure_password
DB_NAME=gym_access
SERVER_PORT=8080
GIN_MODE=release
```

## Monitoring

### Health Check
```bash
curl http://localhost:8080/health
```

### Test API Endpoints
```bash
# Check access
curl -X GET http://localhost:8080/api/v1/access/MEM001 \
  -H "X-API-Key: main-gym-api-key-123"

# Get member changes
curl -X GET "http://localhost:8080/api/v1/members/changes?since=2024-01-01T00:00:00Z" \
  -H "X-API-Key: main-gym-api-key-123"
```

## Troubleshooting

### Terminal can't connect to cloud
- Check `config.json` `api_url` is correct
- Verify API key matches a site in database
- Check network connectivity
- Review terminal logs

### API returns 401 Unauthorized
- Verify `X-API-Key` header is present
- Check API key exists in `sites` table
- Ensure site is marked as `active`

### Sync not working
- Check timestamp format (ISO 8601)
- Verify `updated_at` trigger is working
- Check terminal's last sync time

## Future Enhancements

1. **Admin Portal:** Web interface for member management
2. **Real-time Notifications:** WebSocket for live updates
3. **Advanced Analytics:** Access patterns and usage statistics
4. **Multi-factor Auth:** Additional security layers
5. **Mobile App:** Member self-service portal
6. **Biometric Backup:** Secondary authentication methods
