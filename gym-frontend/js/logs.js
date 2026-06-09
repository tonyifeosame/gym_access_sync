(() => {
  if (!Layout.init("logs", "Access Logs")) return;

  const esc = Layout.escapeHtml;
  const alertBox = document.getElementById("pageAlert");
  const logRows = document.getElementById("logRows");
  const dateFilter = document.getElementById("dateFilter");
  const memberFilter = document.getElementById("memberFilter");
  const logCount = document.getElementById("logCount");

  function showError(msg) {
    alertBox.textContent = msg;
    alertBox.classList.remove("d-none");
  }

  function render(logs) {
    logCount.textContent = logs.length + " record" + (logs.length === 1 ? "" : "s");
    if (!logs.length) {
      logRows.innerHTML =
        '<tr><td colspan="5" class="text-center text-muted py-4">No access logs match the filter.</td></tr>';
      return;
    }
    logRows.innerHTML = logs
      .map(
        (l) => `
        <tr>
          <td>${esc(l.timestamp)}</td>
          <td><code>${esc(l.member_id)}</code></td>
          <td>${Layout.grantedBadge(l.granted)}</td>
          <td>${esc(l.reason)}</td>
          <td><span class="text-muted small">${esc(l.source)}</span></td>
        </tr>`
      )
      .join("");
  }

  async function load() {
    alertBox.classList.add("d-none");
    logRows.innerHTML =
      '<tr><td colspan="5" class="text-center text-muted py-4">Loading...</td></tr>';
    const member = memberFilter.value.trim();
    const date = dateFilter.value;
    try {
      let logs;
      if (member) {
        logs = await API.getMemberLogs(member);
        if (date) logs = logs.filter((l) => (l.timestamp || "").startsWith(date));
      } else {
        logs = await API.getLogs(date);
      }
      render(logs);
    } catch (err) {
      showError(err.message);
      logRows.innerHTML =
        '<tr><td colspan="5" class="text-center text-muted py-4">Could not load logs.</td></tr>';
    }
  }

  document.getElementById("applyBtn").addEventListener("click", load);
  document.getElementById("clearBtn").addEventListener("click", () => {
    dateFilter.value = "";
    memberFilter.value = "";
    load();
  });
  memberFilter.addEventListener("keydown", (e) => {
    if (e.key === "Enter") load();
  });

  load();
})();
