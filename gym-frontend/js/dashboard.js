(() => {
  if (!Layout.init("dashboard", "Dashboard")) return;

  const STAT_DEFS = [
    { key: "total_members", label: "Total Members", icon: "people-fill", color: "#0d6efd" },
    { key: "active_members", label: "Active Members", icon: "person-check-fill", color: "#198754" },
    { key: "inactive_members", label: "Inactive Members", icon: "person-dash-fill", color: "#6c757d" },
    { key: "pending_enrollments", label: "Pending Enrollments", icon: "hourglass-split", color: "#fd7e14" },
    { key: "today_entries", label: "Today's Entries", icon: "door-open-fill", color: "#6610f2" },
  ];

  const alertBox = document.getElementById("pageAlert");

  function showError(msg) {
    alertBox.textContent = msg;
    alertBox.classList.remove("d-none");
  }

  function renderStats(stats) {
    const html = STAT_DEFS.map((s) => {
      const value = stats[s.key] ?? 0;
      return `
        <div class="col-6 col-md-4 col-xl">
          <div class="card stat-card h-100">
            <div class="card-body d-flex align-items-center gap-3">
              <div class="stat-icon" style="background:${s.color}">
                <i class="bi bi-${s.icon}"></i>
              </div>
              <div>
                <div class="stat-value">${value}</div>
                <div class="stat-label">${s.label}</div>
              </div>
            </div>
          </div>
        </div>`;
    }).join("");
    document.getElementById("statCards").innerHTML = html;
  }

  function renderLogs(logs) {
    const body = document.getElementById("recentLogs");
    if (!logs.length) {
      body.innerHTML =
        '<tr><td colspan="5" class="text-center text-muted py-4">No access activity yet.</td></tr>';
      return;
    }
    body.innerHTML = logs
      .slice(0, 8)
      .map(
        (l) => `
        <tr>
          <td>${Layout.escapeHtml(l.timestamp)}</td>
          <td><code>${Layout.escapeHtml(l.member_id)}</code></td>
          <td>${Layout.grantedBadge(l.granted)}</td>
          <td>${Layout.escapeHtml(l.reason)}</td>
          <td><span class="text-muted small">${Layout.escapeHtml(l.source)}</span></td>
        </tr>`
      )
      .join("");
  }

  async function load() {
    try {
      const [stats, logs] = await Promise.all([API.getStats(), API.getLogs()]);
      renderStats(stats);
      renderLogs(logs);
    } catch (err) {
      showError(err.message);
      document.getElementById("recentLogs").innerHTML =
        '<tr><td colspan="5" class="text-center text-muted py-4">Could not load data.</td></tr>';
    }
  }

  load();
})();
