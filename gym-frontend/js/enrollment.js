(() => {
  if (!Layout.init("enrollment", "Enrollment")) return;

  const esc = Layout.escapeHtml;
  const alertBox = document.getElementById("pageAlert");
  const select = document.getElementById("memberSelect");
  const pendingRows = document.getElementById("pendingRows");

  function showError(msg) {
    alertBox.textContent = msg;
    alertBox.classList.remove("d-none");
  }
  function clearError() {
    alertBox.classList.add("d-none");
  }

  async function loadMembers() {
    try {
      const members = await API.getMembers();
      const selectable = members.filter((m) => m.member_status !== "ACTIVE");
      if (!selectable.length) {
        select.innerHTML = '<option value="">No members awaiting enrollment</option>';
      } else {
        select.innerHTML =
          '<option value="">Select a member...</option>' +
          selectable
            .map(
              (m) =>
                `<option value="${esc(m.member_id)}">${esc(m.member_name)} (${esc(m.member_id)})</option>`
            )
            .join("");
      }
    } catch (err) {
      showError(err.message);
    }
  }

  async function loadPending() {
    try {
      const pending = await API.getPending();
      if (!pending.length) {
        pendingRows.innerHTML =
          '<tr><td colspan="3" class="text-center text-muted py-4">No pending enrollments.</td></tr>';
        return;
      }
      pendingRows.innerHTML = pending
        .map(
          (p) => `
        <tr>
          <td><code>${esc(p.member_id)}</code></td>
          <td>${esc(p.member_name)}</td>
          <td class="text-end">
            <button class="btn btn-sm btn-success" data-id="${esc(p.member_id)}">
              <i class="bi bi-check2-circle"></i> Simulate Scan &amp; Activate
            </button>
          </td>
        </tr>`
        )
        .join("");
    } catch (err) {
      showError(err.message);
      pendingRows.innerHTML =
        '<tr><td colspan="3" class="text-center text-muted py-4">Could not load.</td></tr>';
    }
  }

  async function refresh() {
    clearError();
    await Promise.all([loadMembers(), loadPending()]);
  }

  document.getElementById("startBtn").addEventListener("click", async () => {
    clearError();
    const id = select.value;
    if (!id) return showError("Please select a member first.");
    try {
      await API.startEnrollment(id);
      Layout.toast("Enrollment request sent for " + id + ".");
      await refresh();
    } catch (err) {
      showError(err.message);
    }
  });

  pendingRows.addEventListener("click", async (e) => {
    const btn = e.target.closest("button[data-id]");
    if (!btn) return;
    const id = btn.dataset.id;
    btn.disabled = true;
    try {
      // Simulated fingerprint template (the F22 would supply the real one).
      const template = "SIMULATED-" + id + "-" + Date.now();
      await API.submitEnrollment(id, template);
      Layout.toast(id + " enrolled and activated.");
      await refresh();
    } catch (err) {
      showError(err.message);
      btn.disabled = false;
    }
  });

  document.getElementById("refreshBtn").addEventListener("click", refresh);

  refresh();
})();
