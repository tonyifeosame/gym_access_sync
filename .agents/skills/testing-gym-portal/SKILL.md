---
name: testing-gym-portal
description: Test the gym access admin portal (Bootstrap frontend + C++/SQLite server) end-to-end. Use when verifying the gym-frontend UI or the crow_server.cpp routes (members, enrollment, access logs, stats).
---

# Testing the Gym Access Admin Portal

## Architecture (important — docs are misleading)
The runnable backend is a **C++ raw-socket HTTP server** (`crow_server.cpp`) using **SQLite** (`members.db`), NOT the Go/PostgreSQL stack described in `ARCHITECTURE.md`. There is **no `/api/v1` prefix**. The frontend is plain Bootstrap 5 + vanilla JS in `gym-frontend/` (no build step).

## Auth
- Single static API key sent as header `X-API-Key: gym-secret-key`. There is no login endpoint — the login page just verifies the key by calling `GET /stats` and stores `{baseUrl, apiKey}` in `localStorage` under `gym_api_config`.
- Default dev values: API Server URL `http://localhost:8080`, API Key `gym-secret-key`.

## Build & run
```bash
# Backend (build once, then run). Compiled binary + members.db can live in a separate dir.
g++ -std=c++17 crow_server.cpp database.cpp -o gym_server -lsqlite3
./gym_server          # serves on :8080, auto-creates members.db (+ seed data on first run)

# Frontend (static server, separate shell)
cd gym-frontend && python3 -m http.server 8000
```
Then open `http://localhost:8000/index.html`.

Note: servers run as background processes — after any VM/session restart they are NOT running; restart both. Verify with:
```bash
curl -s -H "X-API-Key: gym-secret-key" http://localhost:8080/stats
curl -s -o /dev/null -w "%{http_code}\n" http://localhost:8000/index.html
```

## Key endpoints
- `GET /stats` — dashboard counts {total, active, inactive, pending, today_entries}
- `GET/POST /members`, `GET/PUT/DELETE /members/{id}`
- `POST /enrollment/start`, `POST /enrollment/result` (Simulate Scan activates a member)
- `GET /enrollment/pending`
- `GET /access/logs` (optional `?date=`), `GET /access/logs/{member_id}`, `POST /access/log`
- `GET /access/{member_id}` — access check (auto-writes an access log)

## Primary end-to-end test flow (UI)
1. Login: invalid key → red "Invalid API key." and no redirect; valid key → Dashboard.
2. Dashboard: 5 stat cards show numeric counts (proves `/stats`).
3. Members → Add Member: fill Member ID (e.g. MEM100), Full Name, expiring date → Save. Expect "Member created." toast + new PENDING ENROLLMENT row. (Regression guard: `POST /members` previously failed because `member_id` wasn't parsed.)
4. Enrollment: new member appears in Pending → click "Simulate Scan & Activate" → toast, row leaves Pending, Members shows status ACTIVE.
5. Access Logs: rows show GRANTED/DENIED badges + timestamps + source; Member-ID filter with a nonexistent ID → "0 records"; Clear restores list.
6. Dashboard: Total +1, Active +1 after the above.

## Member status values
`ACTIVE`, `INACTIVE`, `PENDING_ENROLLMENT` (rendered as ACTIVE / INACTIVE / PENDING ENROLLMENT badges).

## Gotchas
- CORS: the server must send `Access-Control-Allow-*` headers and handle `OPTIONS` preflight because the frontend (:8000) and backend (:8080) are different origins. If member creation/login silently fails in the browser, check the console for a CORS/preflight error.
- No backend yet for: F22 terminal online/offline status, and member phone/email/membership_type — these UI areas are placeholders and not testable.
- To reset test data, stop the server and delete `members.db`, then restart (re-seeds).

## Devin Secrets Needed
None. The API key is a static, non-sensitive dev key (`gym-secret-key`).
