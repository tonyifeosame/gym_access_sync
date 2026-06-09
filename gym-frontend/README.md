# Gym Access — Admin Portal (Frontend)

A lightweight staff admin portal for the Gym Access System, built with plain
**HTML + CSS + JavaScript + Bootstrap 5** (no build step, no framework). It talks
directly to the existing C++ REST server (`crow_server.cpp`).

## Pages

| Page             | File             | What it does                                                        |
| ---------------- | ---------------- | ------------------------------------------------------------------- |
| Login            | `index.html`     | Stores the API URL + `X-API-Key` and verifies them via `/stats`.    |
| Dashboard        | `dashboard.html` | Member counts, today's entries, recent access activity.             |
| Members          | `members.html`   | List/search members, Add/Edit/View, Deactivate, Delete, Enroll.    |
| Enrollment       | `enrollment.html`| Send enrollment requests; view & complete pending enrollments.      |
| Access Logs      | `logs.html`      | Browse access logs, filter by date and member.                      |
| Device Status    | `device.html`    | Live cloud-API reachability; F22 terminal status (placeholder).     |

## Running

1. **Start the backend** (from the repo root):

   ```bash
   g++ -std=c++17 -O2 -o gym_server crow_server.cpp database.cpp -lsqlite3 -lpthread
   ./gym_server          # listens on http://localhost:8080
   ```

2. **Serve the frontend** (any static server works):

   ```bash
   cd gym-frontend
   python3 -m http.server 8000
   ```

3. Open <http://localhost:8000> and sign in.
   - **API Server URL:** `http://localhost:8080`
   - **API Key:** `gym-secret-key`

> Opening the HTML files directly via `file://` also works because the backend
> sends permissive CORS headers, but serving over HTTP is recommended.

## Auth model

There is no user/password backend — the C++ server authenticates every request
with a single static `X-API-Key` header. The login screen simply captures and
stores that key (in `localStorage`) and attaches it to all API calls.

## API endpoints used

`GET /stats`, `GET/POST /members`, `GET/PUT/DELETE /members/{id}`,
`GET /access/{id}`, `GET /access/logs`, `GET /access/logs/{id}`, `POST /access/log`,
`GET /enrollment/pending`, `POST /enrollment/start`, `POST /enrollment/result`.
