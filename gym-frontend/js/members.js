(() => {
  if (!Layout.init("members", "Members")) return;

  const esc = Layout.escapeHtml;
  let allMembers = [];
  let editingId = null;

  const alertBox = document.getElementById("pageAlert");
  const rows = document.getElementById("memberRows");
  const searchInput = document.getElementById("searchInput");

  const memberModal = new bootstrap.Modal(document.getElementById("memberModal"));
  const viewModal = new bootstrap.Modal(document.getElementById("viewModal"));
  const modalAlert = document.getElementById("modalAlert");

  function showError(msg) {
    alertBox.textContent = msg;
    alertBox.classList.remove("d-none");
  }

  function render(list) {
    if (!list.length) {
      rows.innerHTML =
        '<tr><td colspan="6" class="text-center text-muted py-4">No members found.</td></tr>';
      return;
    }
    rows.innerHTML = list
      .map((m) => {
        const enrollBtn =
          m.member_status !== "ACTIVE"
            ? `<button class="dropdown-item" data-act="enroll" data-id="${esc(m.member_id)}"><i class="bi bi-fingerprint"></i> Start Enrollment</button>`
            : "";
        const deactivateBtn =
          m.member_status !== "INACTIVE"
            ? `<button class="dropdown-item" data-act="deactivate" data-id="${esc(m.member_id)}"><i class="bi bi-person-dash"></i> Deactivate</button>`
            : "";
        return `
        <tr>
          <td><code>${esc(m.member_id)}</code></td>
          <td>${esc(m.member_name)}</td>
          <td>${Layout.statusBadge(m.member_status)}</td>
          <td>${esc(m.member_expiring_date) || '<span class="text-muted">—</span>'}</td>
          <td><span class="text-muted small">${esc(m.last_updated)}</span></td>
          <td class="text-end">
            <div class="dropdown">
              <button class="btn btn-sm btn-outline-secondary dropdown-toggle" data-bs-toggle="dropdown">Actions</button>
              <div class="dropdown-menu dropdown-menu-end">
                <button class="dropdown-item" data-act="view" data-id="${esc(m.member_id)}"><i class="bi bi-eye"></i> View</button>
                <button class="dropdown-item" data-act="edit" data-id="${esc(m.member_id)}"><i class="bi bi-pencil"></i> Edit</button>
                ${enrollBtn}
                ${deactivateBtn}
                <div class="dropdown-divider"></div>
                <button class="dropdown-item text-danger" data-act="delete" data-id="${esc(m.member_id)}"><i class="bi bi-trash"></i> Delete</button>
              </div>
            </div>
          </td>
        </tr>`;
      })
      .join("");
  }

  function applyFilter() {
    const q = searchInput.value.trim().toLowerCase();
    if (!q) return render(allMembers);
    render(
      allMembers.filter(
        (m) =>
          (m.member_id || "").toLowerCase().includes(q) ||
          (m.member_name || "").toLowerCase().includes(q) ||
          (m.member_status || "").toLowerCase().includes(q)
      )
    );
  }

  async function load() {
    try {
      allMembers = await API.getMembers();
      applyFilter();
    } catch (err) {
      showError(err.message);
      rows.innerHTML =
        '<tr><td colspan="6" class="text-center text-muted py-4">Could not load members.</td></tr>';
    }
  }

  // ---------- Add / Edit ----------
  function openAdd() {
    editingId = null;
    document.getElementById("memberModalTitle").textContent = "Add Member";
    document.getElementById("memberForm").reset();
    document.getElementById("m_id").disabled = false;
    document.getElementById("m_id_help").style.display = "";
    modalAlert.classList.add("d-none");
    memberModal.show();
  }

  function openEdit(member) {
    editingId = member.member_id;
    document.getElementById("memberModalTitle").textContent = "Edit Member";
    document.getElementById("m_id").value = member.member_id;
    document.getElementById("m_id").disabled = true;
    document.getElementById("m_id_help").style.display = "none";
    document.getElementById("m_name").value = member.member_name || "";
    document.getElementById("m_expiring").value = member.member_expiring_date || "";
    document.getElementById("m_status").value = member.member_status || "PENDING_ENROLLMENT";
    modalAlert.classList.add("d-none");
    memberModal.show();
  }

  document.getElementById("memberForm").addEventListener("submit", async (e) => {
    e.preventDefault();
    modalAlert.classList.add("d-none");
    const payload = {
      member_name: document.getElementById("m_name").value.trim(),
      member_expiring_date: document.getElementById("m_expiring").value,
      member_status: document.getElementById("m_status").value,
    };
    const btn = document.getElementById("memberSaveBtn");
    btn.disabled = true;
    try {
      if (editingId) {
        await API.updateMember(editingId, payload);
        Layout.toast("Member updated.");
      } else {
        payload.member_id = document.getElementById("m_id").value.trim();
        await API.createMember(payload);
        Layout.toast("Member created.");
      }
      memberModal.hide();
      await load();
    } catch (err) {
      modalAlert.textContent = err.message;
      modalAlert.classList.remove("d-none");
    } finally {
      btn.disabled = false;
    }
  });

  // ---------- View ----------
  function showView(m) {
    const row = (label, value) => `
      <div class="row mb-2">
        <div class="col-5 text-muted">${label}</div>
        <div class="col-7">${value}</div>
      </div>`;
    document.getElementById("viewBody").innerHTML =
      row("Member ID", `<code>${esc(m.member_id)}</code>`) +
      row("Full Name", esc(m.member_name)) +
      row("Status", Layout.statusBadge(m.member_status)) +
      row("Expiring Date", esc(m.member_expiring_date) || "—") +
      row("Fingerprint", m.member_fingerprint_template ? "Enrolled" : '<span class="text-warning">Not enrolled</span>') +
      row("Last Updated", esc(m.last_updated));
    viewModal.show();
  }

  // ---------- Row actions ----------
  rows.addEventListener("click", async (e) => {
    const btn = e.target.closest("[data-act]");
    if (!btn) return;
    const id = btn.dataset.id;
    const act = btn.dataset.act;
    const member = allMembers.find((m) => m.member_id === id);

    if (act === "view") return showView(member);
    if (act === "edit") return openEdit(member);

    if (act === "enroll") {
      try {
        await API.startEnrollment(id);
        Layout.toast("Enrollment started for " + id + ".");
        await load();
      } catch (err) {
        showError(err.message);
      }
      return;
    }

    if (act === "deactivate") {
      if (!confirm("Deactivate member " + id + "? They will lose access.")) return;
      try {
        await API.deactivateMember(id);
        Layout.toast("Member deactivated.");
        await load();
      } catch (err) {
        showError(err.message);
      }
      return;
    }

    if (act === "delete") {
      if (!confirm("Permanently delete member " + id + "? This cannot be undone.")) return;
      try {
        await API.deleteMember(id);
        Layout.toast("Member deleted.", "danger");
        await load();
      } catch (err) {
        showError(err.message);
      }
    }
  });

  document.getElementById("addMemberBtn").addEventListener("click", openAdd);
  searchInput.addEventListener("input", applyFilter);

  // Open Add modal automatically if navigated with #add
  if (window.location.hash === "#add") openAdd();

  load();
})();
