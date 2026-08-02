/* MaNGOS conf editor UI — talks to the C++ host via chrome.webview */

(function () {
  "use strict";

  const THEMES = ["light", "dark", "amiga"];
  const THEME_LABEL = { light: "Light", dark: "Dark", amiga: "Amiga" };

  let state = {
    path: "",
    savePath: "",
    groups: [],
    sections: [],
    entries: [],
    activeGroup: 0,
    theme: 0,
    searchFrom: 0
  };

  const $ = (id) => document.getElementById(id);
  const page = $("page");
  const tabs = $("tabs");
  const status = $("status");
  const headerPath = $("headerPath");

  function hostAvailable() {
    return !!(window.chrome && chrome.webview && chrome.webview.postMessage);
  }

  function post(obj) {
    if (!hostAvailable()) {
      setStatus("Not running inside WebView2 host");
      return;
    }
    chrome.webview.postMessage(JSON.stringify(obj));
  }

  function setStatus(text) {
    status.textContent = text || "";
  }

  function collectValues() {
    const values = [];
    for (const e of state.entries) {
      const el = document.querySelector(`[data-idx="${e.i}"]`);
      if (!el) {
        continue;
      }
      let v = e.value;
      if (e.kind === "bool") {
        v = el.checked ? (e.value === "true" || e.value === "false" ? "true" : "1")
                       : (e.value === "true" || e.value === "false" ? "false" : "0");
        // preserve word vs 0/1 from original
        if (e.value === "true" || e.value === "false") {
          v = el.checked ? "true" : "false";
        } else {
          v = el.checked ? "1" : "0";
        }
      } else if (e.kind === "connection") {
        const parts = el.querySelectorAll("input");
        v = Array.from(parts).map((p) => p.value).join(";");
      } else if (e.kind === "choice" || e.kind === "enum") {
        v = el.value;
      } else {
        v = el.value;
      }
      if (v !== e.value) {
        values.push({ i: e.i, v: v });
      }
    }
    return values;
  }

  function markDirtyLocal(values) {
    for (const ch of values) {
      const e = state.entries.find((x) => x.i === ch.i);
      if (e) {
        e.value = ch.v;
      }
    }
  }

  function sendWithValues(cmd) {
    post({ cmd: cmd, values: collectValues() });
  }

  function groupCounts() {
    const counts = state.groups.map(() => 0);
    for (const e of state.entries) {
      if (e.group >= 0 && e.group < counts.length) {
        counts[e.group]++;
      }
    }
    return counts;
  }

  function renderTabs() {
    const counts = groupCounts();
    tabs.innerHTML = "";
    let first = -1;
    state.groups.forEach((name, g) => {
      if (!counts[g]) {
        return;
      }
      if (first < 0) {
        first = g;
      }
      const btn = document.createElement("button");
      btn.type = "button";
      btn.className = "tab";
      btn.setAttribute("role", "tab");
      btn.dataset.group = String(g);
      btn.textContent = `${name} (${counts[g]})`;
      btn.setAttribute("aria-selected", g === state.activeGroup ? "true" : "false");
      btn.addEventListener("click", () => {
        sendWithValues("dirty");
        state.activeGroup = g;
        renderTabs();
        renderPage();
      });
      tabs.appendChild(btn);
    });
    if (first >= 0 && counts[state.activeGroup] === 0) {
      state.activeGroup = first;
      renderTabs();
      renderPage();
    }
  }

  function optionHtml(e) {
    if (e.kind === "enum") {
      let html = "";
      let found = false;
      for (let i = 0; i < e.optionValues.length; i++) {
        const sel = e.optionValues[i] === e.value ? " selected" : "";
        if (sel) {
          found = true;
        }
        const label = `${e.optionValues[i]}  —  ${e.optionLabels[i] || ""}`;
        html += `<option value="${escAttr(e.optionValues[i])}"${sel}>${esc(label)}</option>`;
      }
      if (!found) {
        html = `<option value="${escAttr(e.value)}" selected>${esc(e.value)}</option>` + html;
      }
      return `<select data-idx="${e.i}" title="${escAttr(tip(e))}">${html}</select>`;
    }
    if (e.kind === "choice") {
      let html = "";
      for (const c of e.choices) {
        const sel = c === e.value ? " selected" : "";
        html += `<option value="${escAttr(c)}"${sel}>${esc(c)}</option>`;
      }
      return `<select data-idx="${e.i}" title="${escAttr(tip(e))}">${html}</select>`;
    }
    return "";
  }

  function tip(e) {
    return e.doc ? `${e.key}\n\n${e.doc}` : e.key;
  }

  function fieldHtml(e) {
    const t = escAttr(tip(e));
    if (e.kind === "bool") {
      const on = e.value === "1" || e.value === "true";
      return `<input type="checkbox" data-idx="${e.i}" title="${t}"${on ? " checked" : ""}>`;
    }
    if (e.kind === "enum" || e.kind === "choice") {
      return optionHtml(e);
    }
    if (e.kind === "connection") {
      const parts = (e.value || "").split(";");
      while (parts.length < 5) {
        parts.push("");
      }
      const names = ["Host", "Port", "User", "Password", "Database"];
      let cells = "";
      for (let p = 0; p < 5; p++) {
        cells += `<div class="conn-cell"><div class="cap">${names[p]}</div>` +
          `<input type="text" value="${escAttr(parts[p])}" title="${t}"></div>`;
      }
      return `<div class="conn-parts" data-idx="${e.i}">${cells}</div>`;
    }
    if (e.kind === "number") {
      return `<input type="text" inputmode="decimal" data-idx="${e.i}" value="${escAttr(e.value)}" title="${t}">`;
    }
    // path + text share a text field; host has no folder picker for web path yet
    return `<input type="text" data-idx="${e.i}" value="${escAttr(e.value)}" title="${t}">`;
  }

  function renderPage() {
    const list = state.entries.filter((e) => e.group === state.activeGroup);
    if (!list.length) {
      page.innerHTML = `<div class="empty">No settings on this tab. Open a conf file.</div>`;
      return;
    }

    let html = "";
    let section = -1;
    let openGrid = false;

    const closeGrid = () => {
      if (openGrid) {
        html += "</div>";
        openGrid = false;
      }
    };

    for (const e of list) {
      if (e.section !== section) {
        closeGrid();
        section = e.section;
        const title = (state.sections[section] && state.sections[section].title) || "General";
        html += `<h2 class="section-title">${esc(title)}</h2><div class="grid">`;
        openGrid = true;
      }
      const conn = e.kind === "connection" ? " connection" : "";
      html += `<div class="row${conn}" id="row-${e.i}">` +
        `<label class="lbl" title="${escAttr(tip(e))}">${esc(e.label || e.key)}</label>` +
        `<div class="field-wrap">${fieldHtml(e)}</div></div>`;
    }
    closeGrid();
    page.innerHTML = html;

    const counts = groupCounts();
    setStatus(`${list.length} settings on this tab — ${state.entries.length} in the file`);
  }

  function esc(s) {
    return String(s == null ? "" : s)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;");
  }

  function escAttr(s) {
    return esc(s).replace(/\n/g, "&#10;");
  }

  function applyTheme() {
    const name = THEMES[state.theme] || "light";
    document.body.setAttribute("data-theme", name);
    $("btnTheme").textContent = "Theme: " + (THEME_LABEL[name] || name);
  }

  function onLoaded(msg) {
    state.path = msg.path || "";
    state.savePath = msg.savePath || state.path;
    state.groups = msg.groups || [];
    state.sections = msg.sections || [];
    state.entries = msg.entries || [];
    state.searchFrom = 0;

    const counts = groupCounts();
    state.activeGroup = counts.findIndex((c) => c > 0);
    if (state.activeGroup < 0) {
      state.activeGroup = 0;
    }

    headerPath.textContent = state.path || "";
    if (msg.placeholders > 0) {
      setStatus(`${msg.placeholders} value(s) are still CMake placeholders (@NAME@)`);
    } else if (state.savePath && state.savePath !== state.path) {
      setStatus(`Read template — Save writes ${baseName(state.savePath)}`);
    }

    renderTabs();
    renderPage();
  }

  function baseName(p) {
    const i = Math.max(p.lastIndexOf("\\"), p.lastIndexOf("/"));
    return i >= 0 ? p.slice(i + 1) : p;
  }

  function doFind() {
    const q = ($("search").value || "").trim().toLowerCase();
    if (!q) {
      return;
    }
    const n = state.entries.length;
    if (!n) {
      return;
    }
    for (let step = 0; step < n; step++) {
      const idx = (state.searchFrom + step) % n;
      const e = state.entries[idx];
      const hay = `${e.key} ${e.label || ""} ${e.doc || ""}`.toLowerCase();
      if (hay.indexOf(q) < 0) {
        continue;
      }
      state.searchFrom = idx + 1;
      if (e.group !== state.activeGroup) {
        state.activeGroup = e.group;
        renderTabs();
        renderPage();
      }
      const row = document.getElementById("row-" + e.i);
      if (row) {
        row.scrollIntoView({ block: "center", behavior: "smooth" });
        row.classList.add("highlight");
        setTimeout(() => row.classList.remove("highlight"), 1600);
        const focusable = row.querySelector("input,select");
        if (focusable) {
          focusable.focus();
        }
      }
      setStatus("Found " + e.key);
      return;
    }
    setStatus("No setting matches “" + q + "”");
  }

  function onHostMessage(msg) {
    if (!msg || !msg.cmd) {
      return;
    }
    if (msg.cmd === "loaded") {
      onLoaded(msg);
      return;
    }
    if (msg.cmd === "status") {
      setStatus(msg.text || "");
      return;
    }
    if (msg.cmd === "error") {
      setStatus(msg.text || "Error");
      alert(msg.text || "Error");
      return;
    }
    if (msg.cmd === "saved") {
      state.path = msg.path || state.path;
      state.savePath = msg.path || state.savePath;
      headerPath.textContent = state.path;
      // refresh local values already applied
      return;
    }
  }

  $("btnOpen").addEventListener("click", () => post({ cmd: "open" }));
  $("btnSave").addEventListener("click", () => sendWithValues("save"));
  $("btnSaveAs").addEventListener("click", () => sendWithValues("saveas"));
  $("btnReload").addEventListener("click", () => sendWithValues("reload"));
  $("btnFind").addEventListener("click", doFind);
  $("search").addEventListener("keydown", (ev) => {
    if (ev.key === "Enter") {
      doFind();
    }
  });
  $("btnTheme").addEventListener("click", () => {
    state.theme = (state.theme + 1) % THEMES.length;
    applyTheme();
    setStatus("Theme: " + THEME_LABEL[THEMES[state.theme]]);
  });

  if (hostAvailable()) {
    chrome.webview.addEventListener("message", (ev) => {
      let data = ev.data;
      if (typeof data === "string") {
        try {
          data = JSON.parse(data);
        } catch (e) {
          return;
        }
      }
      onHostMessage(data);
    });
  }

  applyTheme();
  post({ cmd: "ready" });
})();
