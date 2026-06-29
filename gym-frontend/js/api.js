/*
 * api.js - central API client for the Gym Access admin portal.
 * Talks to the C++ REST server (crow_server.cpp), default http://localhost:8080.
 * Auth is a static API key sent in the X-API-Key header.
 */
const API = (() => {
  const STORAGE_KEY = "gym_api_config";

  const DEFAULTS = {
    baseUrl: "http://localhost:8080",
    apiKey: "gym-secret-key",
  };

  function getConfig() {
    try {
      const raw = localStorage.getItem(STORAGE_KEY);
      if (raw) return JSON.parse(raw);
    } catch (e) {
      /* ignore */
    }
    return null;
  }

  function setConfig(baseUrl, apiKey) {
    const cfg = { baseUrl: baseUrl.replace(/\/+$/, ""), apiKey };
    localStorage.setItem(STORAGE_KEY, JSON.stringify(cfg));
    return cfg;
  }

  function clearConfig() {
    localStorage.removeItem(STORAGE_KEY);
  }

  function isAuthenticated() {
    const cfg = getConfig();
    return !!(cfg && cfg.baseUrl && cfg.apiKey);
  }

  async function request(method, path, body) {
    const cfg = getConfig() || DEFAULTS;
    const opts = {
      method,
      headers: { "X-API-Key": cfg.apiKey },
    };
    if (body !== undefined) {
      opts.headers["Content-Type"] = "application/json";
      opts.body = JSON.stringify(body);
    }

    let res;
    try {
      res = await fetch(cfg.baseUrl + path, opts);
    } catch (err) {
      throw new Error(
        "Cannot reach the API server at " +
          cfg.baseUrl +
          ". Is the C++ server running?"
      );
    }

    if (res.status === 401) {
      throw new Error("Unauthorized - invalid API key.");
    }

    let data = {};
    const text = await res.text();
    if (text) {
      try {
        data = JSON.parse(text);
      } catch (e) {
        throw new Error("Invalid response from server.");
      }
    }

    if (!res.ok || data.success === false) {
      throw new Error(data.error || "Request failed (HTTP " + res.status + ")");
    }
    return data;
  }

  return {
    DEFAULTS,
    getConfig,
    setConfig,
    clearConfig,
    isAuthenticated,

    // Auth helper - verifies credentials by calling /stats.
    login: (baseUrl, apiKey) => {
      const cleaned = baseUrl.replace(/\/+$/, "");
      return fetch(cleaned + "/stats", { headers: { "X-API-Key": apiKey } }).then(
        (res) => {
          if (res.status === 401) throw new Error("Invalid API key.");
          if (!res.ok) throw new Error("Server error (HTTP " + res.status + ")");
          setConfig(baseUrl, apiKey);
          return true;
        },
        () => {
          throw new Error("Cannot reach the server at " + cleaned + ".");
        }
      );
    },

    // Stats
    getStats: () => request("GET", "/stats").then((d) => d.stats),

    // Members
    getMembers: () => request("GET", "/members").then((d) => d.members || []),
    getMember: (id) =>
      request("GET", "/members/" + encodeURIComponent(id)).then((d) => d.member),
    createMember: (member) => request("POST", "/members", member),
    updateMember: (id, member) =>
      request("PUT", "/members/" + encodeURIComponent(id), member),
    deleteMember: (id) =>
      request("DELETE", "/members/" + encodeURIComponent(id)),
    deactivateMember: (id) =>
      request("PUT", "/members/" + encodeURIComponent(id), {
        member_status: "INACTIVE",
      }),

    // Access
    checkAccess: (id) => request("GET", "/access/" + encodeURIComponent(id)),
    // Evaluate access and write a check-in row to access_logs.
    checkIn: (id) =>
      request("GET", "/access/" + encodeURIComponent(id)).then((access) =>
        request("POST", "/access/log", {
          member_id: id,
          granted: access.granted,
          reason: access.message,
          source: "admin-portal-check-in",
        }).then(() => access)
      ),
    getLogs: (date) =>
      request("GET", "/access/logs" + (date ? "?date=" + encodeURIComponent(date) : "")).then(
        (d) => d.logs || []
      ),
    getMemberLogs: (id) =>
      request("GET", "/access/logs/" + encodeURIComponent(id)).then(
        (d) => d.logs || []
      ),

    // Enrollment
    getPending: () =>
      request("GET", "/enrollment/pending").then((d) => d.pending || []),
    startEnrollment: (id) =>
      request("POST", "/enrollment/start", { member_id: id }),
    submitEnrollment: (id, fingerprint) =>
      request("POST", "/enrollment/result", {
        member_id: id,
        member_fingerprint_template: fingerprint,
      }),
  };
})();
