/*
 * layout.js - shared shell (sidebar + topbar), auth guard, and UI helpers.
 * Each protected page calls Layout.init('<page-key>', 'Page Title').
 */
const Layout = (() => {
  const NAV = [
    { key: "dashboard", label: "Dashboard", icon: "speedometer2", href: "dashboard.html" },
    { key: "members", label: "Members", icon: "people", href: "members.html" },
    { key: "enrollment", label: "Enrollment", icon: "fingerprint", href: "enrollment.html" },
    { key: "logs", label: "Access Logs", icon: "card-list", href: "logs.html" },
    { key: "device", label: "Device Status", icon: "hdd-network", href: "device.html" },
  ];

  function requireAuth() {
    if (!API.isAuthenticated()) {
      window.location.replace("index.html");
      return false;
    }
    return true;
  }

  function renderSidebar(active) {
    const links = NAV.map(
      (n) =>
        `<a class="nav-link ${n.key === active ? "active" : ""}" href="${n.href}">
           <span class="icon"><i class="bi bi-${n.icon}"></i></span>
           <span>${n.label}</span>
         </a>`
    ).join("");
    return `
      <div class="brand">
        <span class="logo-box"><i class="bi bi-shield-lock"></i></span>
        <span>Gym Access</span>
      </div>
      <nav class="nav flex-column">${links}</nav>`;
  }

  function renderTopbar(title) {
    const cfg = API.getConfig() || {};
    return `
      <div class="d-flex align-items-center gap-2">
        <button class="btn btn-sm btn-outline-secondary d-md-none" id="sidebarToggle">
          <i class="bi bi-list"></i>
        </button>
        <h5 class="page-title">${title}</h5>
      </div>
      <div class="d-flex align-items-center gap-3">
        <span class="text-muted small d-none d-sm-inline">
          <i class="bi bi-hdd-network"></i> ${cfg.baseUrl || ""}
        </span>
        <button class="btn btn-sm btn-outline-danger" id="logoutBtn">
          <i class="bi bi-box-arrow-right"></i> Logout
        </button>
      </div>`;
  }

  function init(active, title) {
    if (!requireAuth()) return false;
    const sidebar = document.getElementById("sidebar");
    const topbar = document.getElementById("topbar");
    if (sidebar) sidebar.innerHTML = renderSidebar(active);
    if (topbar) topbar.innerHTML = renderTopbar(title);

    const logoutBtn = document.getElementById("logoutBtn");
    if (logoutBtn) {
      logoutBtn.addEventListener("click", () => {
        API.clearConfig();
        window.location.replace("index.html");
      });
    }
    const toggle = document.getElementById("sidebarToggle");
    if (toggle) {
      toggle.addEventListener("click", () =>
        sidebar.classList.toggle("open")
      );
    }
    return true;
  }

  // ---------- UI helpers ----------
  function statusBadge(status) {
    const map = {
      ACTIVE: "success",
      INACTIVE: "secondary",
      PENDING_ENROLLMENT: "warning",
    };
    const color = map[status] || "light";
    const label = (status || "UNKNOWN").replace(/_/g, " ");
    return `<span class="badge text-bg-${color} badge-status">${label}</span>`;
  }

  function grantedBadge(granted) {
    return granted
      ? '<span class="badge text-bg-success badge-status">GRANTED</span>'
      : '<span class="badge text-bg-danger badge-status">DENIED</span>';
  }

  function escapeHtml(str) {
    if (str === null || str === undefined) return "";
    return String(str)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;");
  }

  function toast(message, type = "success") {
    let holder = document.getElementById("toastHolder");
    if (!holder) {
      holder = document.createElement("div");
      holder.id = "toastHolder";
      holder.className = "toast-container position-fixed top-0 end-0 p-3";
      holder.style.zIndex = "1080";
      document.body.appendChild(holder);
    }
    const el = document.createElement("div");
    el.className = `toast align-items-center text-bg-${type} border-0`;
    el.setAttribute("role", "alert");
    el.innerHTML = `
      <div class="d-flex">
        <div class="toast-body">${escapeHtml(message)}</div>
        <button type="button" class="btn-close btn-close-white me-2 m-auto" data-bs-dismiss="toast"></button>
      </div>`;
    holder.appendChild(el);
    const t = new bootstrap.Toast(el, { delay: 3500 });
    t.show();
    el.addEventListener("hidden.bs.toast", () => el.remove());
  }

  return { init, statusBadge, grantedBadge, escapeHtml, toast };
})();
