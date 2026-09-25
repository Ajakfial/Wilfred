(() => {
  const launcher = document.getElementById("launcher");
  const input = document.getElementById("q");
  const resultsEl = document.getElementById("results");
  const hint = document.getElementById("hint");

  const params = new URLSearchParams(location.search);
  if (params.has("preview")) document.documentElement.classList.add("preview");

  let items = [];
  let sel = 0;
  let seq = 0;
  let visible = false;
  let hideTimer = 0;

  const badges = {
    application: "bi-app",
    executable: "bi-play-fill",
    directory: "bi-folder2",
    document: "bi-file-earmark-text",
    image: "bi-image",
    video: "bi-camera-video",
    audio: "bi-music-note-beamed",
    archive: "bi-file-earmark-zip",
    source: "bi-code-slash",
    config: "bi-gear",
    shortcut: "bi-box-arrow-up-right",
    browser: "bi-globe",
    calc: "bi-calculator",
    convert: "bi-rulers",
    web: "bi-search",
    file: "bi-file-earmark",
  };

  function nativeSend(msg) {
    const obj = typeof msg === "string" ? JSON.parse(msg) : msg;
    if (window.chrome && window.chrome.webview) {
      window.chrome.webview.postMessage(obj);
      return;
    }
    if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.wilfred) {
      window.webkit.messageHandlers.wilfred.postMessage(JSON.stringify(obj));
    }
  }

  function reportSize() {
    const shell = document.querySelector(".shell");
    nativeSend({
      type: "resize",
      width: Math.ceil(shell.scrollWidth),
      height: Math.ceil(shell.scrollHeight),
    });
  }

  function kindOf(item) {
    if (item.action === "calc") return "calc";
    if (item.action === "convert") return "convert";
    if (item.action === "web") return "web";
    return item.kind || "file";
  }

  function render() {
    if (!items.length) {
      resultsEl.hidden = true;
      resultsEl.innerHTML = "";
      hint.textContent = "esc";
      reportSize();
      return;
    }
    resultsEl.hidden = false;
    hint.textContent = "↵";
    resultsEl.innerHTML = items
      .map((item, i) => {
        const k = kindOf(item);
        const badge = badges[k] || badges.file;
        const sub = item.subtitle || item.path || "";
        return `<div class="row${i === sel ? " is-sel" : ""}" data-i="${i}">
          <div class="badge"><i class="bi ${badge}" aria-hidden="true"></i></div>
          <div class="meta">
            <div class="title"></div>
            <div class="sub"></div>
          </div>
          <div class="kind"></div>
        </div>`;
      })
      .join("");
    [...resultsEl.children].forEach((row, i) => {
      row.querySelector(".title").textContent = items[i].title || "";
      row.querySelector(".sub").textContent = items[i].subtitle || items[i].path || "";
      row.querySelector(".kind").textContent = kindOf(items[i]);
    });
    reportSize();
  }

  function submit() {
    if (!items.length) return;
    nativeSend({ type: "submit", index: sel });
  }

  function dismiss() {
    if (!visible) return;
    visible = false;
    launcher.classList.remove("is-in");
    launcher.classList.add("is-out");
    launcher.setAttribute("aria-hidden", "true");
    clearTimeout(hideTimer);
    hideTimer = setTimeout(() => nativeSend({ type: "hidden" }), 200);
  }

  function show() {
    clearTimeout(hideTimer);
    visible = true;
    items = [];
    sel = 0;
    input.value = "";
    launcher.classList.remove("is-out");
    launcher.classList.add("is-in");
    launcher.setAttribute("aria-hidden", "false");
    render();
    requestAnimationFrame(() => {
      input.focus();
      reportSize();
    });
  }

  function onNative(msg) {
    if (!msg || typeof msg !== "object") return;
    if (msg.type === "show") show();
    if (msg.type === "hide") dismiss();
    if (msg.type === "results") {
      items = Array.isArray(msg.items) ? msg.items : [];
      sel = 0;
      render();
    }
  }

  window.__wilfredNative = onNative;

  if (window.chrome && window.chrome.webview) {
    window.chrome.webview.addEventListener("message", (e) => {
      const data = typeof e.data === "string" ? JSON.parse(e.data) : e.data;
      onNative(data);
    });
  }

  input.addEventListener("input", () => {
    const q = input.value;
    const id = ++seq;
    nativeSend({ type: "query", q, id });
    if (!window.chrome && !window.webkit) demoQuery(q, id);
  });

  input.addEventListener("keydown", (e) => {
    if (e.key === "Escape") {
      e.preventDefault();
      dismiss();
    } else if (e.key === "ArrowDown") {
      e.preventDefault();
      if (items.length) sel = (sel + 1) % items.length;
      render();
    } else if (e.key === "ArrowUp") {
      e.preventDefault();
      if (items.length) sel = (sel - 1 + items.length) % items.length;
      render();
    } else if (e.key === "Enter") {
      e.preventDefault();
      submit();
    }
  });

  resultsEl.addEventListener("mousedown", (e) => {
    const row = e.target.closest(".row");
    if (!row) return;
    sel = Number(row.dataset.i);
    submit();
  });

  function demoConvert(q) {
    const raw = q.trim();
    const m = raw.match(/^(?:convert\s+)?(-?\d+(?:\.\d+)?)\s*([a-z°µμ/%0-9]+)\s+(?:to|in)\s+([a-z°µμ/%0-9]+)\s*$/i);
    if (!m) return null;
    const value = Number(m[1]);
    const from = m[2].toLowerCase().replace(/[\s.\-]/g, "");
    const to = m[3].toLowerCase().replace(/[\s.\-]/g, "");
    const table = {
      nm: ["L", 1e-9], um: ["L", 1e-6], mm: ["L", 1e-3], cm: ["L", 0.01], dm: ["L", 0.1],
      m: ["L", 1], km: ["L", 1000], in: ["L", 0.0254], inch: ["L", 0.0254], inches: ["L", 0.0254],
      ft: ["L", 0.3048], foot: ["L", 0.3048], feet: ["L", 0.3048], yd: ["L", 0.9144],
      mi: ["L", 1609.344], mile: ["L", 1609.344], miles: ["L", 1609.344],
      mg: ["M", 1e-6], g: ["M", 0.001], kg: ["M", 1], t: ["M", 1000], tonne: ["M", 1000],
      lb: ["M", 0.45359237], lbs: ["M", 0.45359237], oz: ["M", 0.028349523125],
      ml: ["V", 1e-6], l: ["V", 0.001], liter: ["V", 0.001], liters: ["V", 0.001],
      litre: ["V", 0.001], m3: ["V", 1], gal: ["V", 0.003785411784],
      m2: ["A", 1], km2: ["A", 1e6], ha: ["A", 1e4], acre: ["A", 4046.8564224],
      c: ["T", 1, 273.15], celsius: ["T", 1, 273.15],
      f: ["T", 5 / 9, 273.15 - 32 * 5 / 9], fahrenheit: ["T", 5 / 9, 273.15 - 32 * 5 / 9],
      k: ["T", 1, 0], kelvin: ["T", 1, 0],
      "m/s": ["S", 1], "km/h": ["S", 1000 / 3600], kph: ["S", 1000 / 3600], mph: ["S", 1609.344 / 3600],
      j: ["E", 1], kj: ["E", 1000], cal: ["E", 4.184], kcal: ["E", 4184],
      pa: ["P", 1], kpa: ["P", 1000], bar: ["P", 1e5], atm: ["P", 101325], psi: ["P", 6894.757293168],
      s: ["t", 1], min: ["t", 60], h: ["t", 3600], hour: ["t", 3600],
      kb: ["D", 1024], mb: ["D", 1024 ** 2], gb: ["D", 1024 ** 3],
    };
    const a = table[from];
    const b = table[to];
    if (!a || !b || a[0] !== b[0]) return null;
    const si = value * a[1] + (a[2] || 0);
    const dest = (si - (b[2] || 0)) / b[1];
    if (!Number.isFinite(dest)) return null;
    return {
      title: `${dest} ${m[3]}`,
      subtitle: `Metric conversion · ${raw}`,
      action: "convert",
      kind: "unknown",
    };
  }

  function demoQuery(q, id) {
    const conv = demoConvert(q);
    if (conv) {
      setTimeout(() => {
        if (id !== seq) return;
        onNative({ type: "results", items: [conv] });
      }, 40);
      return;
    }
    const sample = [
      { title: "Firefox", path: "C:\\Program Files\\Firefox\\firefox.exe", kind: "application" },
      { title: "Projects", path: "C:\\Users\\jayla\\Projects", kind: "directory" },
      { title: "notes.md", path: "C:\\Users\\jayla\\Documents\\notes.md", kind: "document" },
      { title: "25 × 42", subtitle: "1050", action: "calc", kind: "unknown" },
    ];
    const needle = q.trim().toLowerCase();
    const next = needle
      ? sample.filter((x) => (x.title + x.path).toLowerCase().includes(needle))
      : [];
    setTimeout(() => {
      if (id !== seq) return;
      onNative({ type: "results", items: next });
    }, 40);
  }

  nativeSend({ type: "ready" });
  if (params.has("preview")) show();
})();
