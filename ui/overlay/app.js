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
    clipboard: "bi-clipboard",
    macro: "bi-lightning",
    mini: "bi-stars",
    weather: "bi-cloud-sun",
    time: "bi-clock",
    disk: "bi-device-hdd",
    disku: "bi-device-hdd",
    ram: "bi-memory",
    cpu: "bi-cpu",
    process: "bi-activity",
    battery: "bi-battery-half",
    host: "bi-pc",
    ip: "bi-ethernet",
    uptime: "bi-hourglass-split",
    user: "bi-person",
    clips: "bi-clipboard-plus",
    os: "bi-windows",
    cores: "bi-cpu-fill",
    screen: "bi-display",
    swap: "bi-layers",
    help: "bi-question-circle",
    content: "bi-file-text",
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
      width: Math.ceil(shell.offsetWidth),
      height: Math.ceil(shell.offsetHeight),
    });
  }

  function kindOf(item) {
    if (item.action === "calc") return "calc";
    if (item.action === "convert") return "convert";
    if (item.action === "web") return "web";
    if (item.action === "habit") return "habit";
    if (item.category === "clipboard" || item.kind === "clipboard") return "clipboard";
    if (item.category === "macro" || item.kind === "macro") return "macro";
    if (item.category === "mini") return item.kind || "mini";
    if (item.category === "content") return "content";
    return item.kind || "file";
  }

  function rowsOf() {
    return items
      .map((item, index) => ({ item, index }))
      .filter((x) => x.item.action !== "habit" || x.item.category === "mini");
  }

  function habitsOf() {
    return items.filter((item) => item.action === "habit" && item.category !== "mini");
  }

  function fillHighlight(el, text, needle) {
    el.textContent = "";
    if (!text) return;
    if (!needle) {
      el.textContent = text;
      return;
    }
    const lower = text.toLowerCase();
    const n = needle.toLowerCase();
    let i = 0;
    let pos = lower.indexOf(n);
    if (pos < 0) {
      el.textContent = text;
      return;
    }
    while (pos >= 0) {
      if (pos > i) el.append(text.slice(i, pos));
      const mark = document.createElement("mark");
      mark.textContent = text.slice(pos, pos + n.length);
      el.append(mark);
      i = pos + n.length;
      pos = lower.indexOf(n, i);
    }
    if (i < text.length) el.append(text.slice(i));
  }

  function paintSelection() {
    const rows = [...resultsEl.querySelectorAll(".row")];
    rows.forEach((row, i) => row.classList.toggle("is-sel", i === sel));
    const cur = rows[sel];
    if (cur) cur.scrollIntoView({ block: "nearest" });
  }

  function render() {
    const habits = habitsOf();
    const rows = rowsOf();
    if (!habits.length && !rows.length) {
      resultsEl.hidden = true;
      resultsEl.innerHTML = "";
      hint.textContent = "esc";
      reportSize();
      return;
    }
    resultsEl.hidden = false;
    hint.textContent = rows.length ? "↵" : "esc";
    const needle = input.value.trim();
    if (sel >= rows.length) sel = 0;
    let html = "";
    if (habits.length) {
      html += `<div class="habits">${habits
        .map(
          (h, i) =>
            `<button type="button" class="habit" data-habit="${i}"></button>`
        )
        .join("")}</div>`;
    }
    if (rows.length) {
      html += `<div class="results-head"><span>${rows.length} result${
        rows.length === 1 ? "" : "s"
      }</span></div>`;
      html += `<div class="rows">${rows
        .map(
          (entry, i) => `<div class="row${i === sel ? " is-sel" : ""}${
            entry.item.category === "mini" ? " is-mini" : ""
          }${entry.item.category === "clipboard" ? " is-clip" : ""}" data-i="${entry.index}">
          <div class="badge"></div>
          <div class="meta">
            <div class="title"></div>
            <div class="sub"></div>
            ${
              Number.isFinite(entry.item.meter) && entry.item.meter >= 0
                ? `<div class="meter"><span style="width:${Math.max(
                    0,
                    Math.min(100, entry.item.meter)
                  )}%"></span></div>`
                : ""
            }
          </div>
          <div class="kind"></div>
        </div>`
        )
        .join("")}</div>`;
    }
    resultsEl.innerHTML = html;
    resultsEl.querySelectorAll(".habit").forEach((btn, i) => {
      btn.textContent = habits[i].title || "";
    });
    [...resultsEl.querySelectorAll(".row")].forEach((row, i) => {
      const item = rows[i].item;
      const k = kindOf(item);
      const badge = row.querySelector(".badge");
      if (item.icon) {
        const img = document.createElement("img");
        img.className = "icon";
        img.alt = "";
        img.src = item.icon;
        badge.append(img);
      } else {
        const ic = document.createElement("i");
        ic.className = "bi " + (badges[k] || badges.file);
        ic.setAttribute("aria-hidden", "true");
        badge.append(ic);
      }
      fillHighlight(row.querySelector(".title"), item.title || "", needle);
      row.querySelector(".sub").textContent = item.subtitle || item.path || "";
      row.querySelector(".kind").textContent = k === "habit" ? "" : k;
    });
    paintSelection();
    reportSize();
  }

  function applyHabit(text) {
    input.value = text;
    input.focus();
    const q = input.value;
    const id = ++seq;
    nativeSend({ type: "query", q, id });
    if (!window.chrome && !window.webkit) demoQuery(q, id);
  }

  function submit() {
    const rows = rowsOf();
    if (!rows.length) return;
    const item = rows[sel].item;
    if (item.action === "habit") {
      applyHabit((item.payload || item.title || "").trimEnd() + " ");
      return;
    }
    nativeSend({ type: "submit", index: rows[sel].index });
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
      const id = ++seq;
      nativeSend({ type: "query", q: "", id });
      if (!window.chrome && !window.webkit) demoQuery("", id);
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
      const n = rowsOf().length;
      if (n) {
        sel = (sel + 1) % n;
        paintSelection();
      }
    } else if (e.key === "ArrowUp") {
      e.preventDefault();
      const n = rowsOf().length;
      if (n) {
        sel = (sel - 1 + n) % n;
        paintSelection();
      }
    } else if (e.key === "Enter") {
      e.preventDefault();
      submit();
    }
  });

  resultsEl.addEventListener("mousedown", (e) => {
    const habit = e.target.closest(".habit");
    if (habit) {
      e.preventDefault();
      const habits = habitsOf();
      const i = Number(habit.dataset.habit);
      if (habits[i]) applyHabit(habits[i].title);
      return;
    }
    const row = e.target.closest(".row");
    if (!row) return;
    const rows = rowsOf();
    const idx = rows.findIndex((x) => x.index === Number(row.dataset.i));
    if (idx >= 0) sel = idx;
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
      { title: "Firefox", path: "C:\\Program Files\\Firefox\\firefox.exe", kind: "application", subtitle: "C:\\Program Files\\Firefox" },
      { title: "File Explorer", path: "C:\\Windows\\explorer.exe", kind: "application", subtitle: "C:\\Windows" },
      { title: "Visual Studio Code", path: "C:\\Users\\jayla\\AppData\\Local\\Programs\\Microsoft VS Code\\Code.exe", kind: "application", subtitle: "Microsoft VS Code" },
      { title: "Projects", path: "C:\\Users\\jayla\\Projects", kind: "directory", subtitle: "C:\\Users\\jayla" },
      { title: "Downloads", path: "C:\\Users\\jayla\\Downloads", kind: "directory", subtitle: "C:\\Users\\jayla" },
      { title: "notes.md", path: "C:\\Users\\jayla\\Documents\\notes.md", kind: "document", subtitle: "Documents" },
      { title: "readme.md", path: "C:\\Users\\jayla\\Downloads\\Wilfred\\README.md", kind: "document", subtitle: "Wilfred" },
      { title: "engine.cpp", path: "C:\\Users\\jayla\\Downloads\\Wilfred\\src\\search\\engine.cpp", kind: "source", subtitle: "search" },
      { title: "photo.png", path: "C:\\Users\\jayla\\Pictures\\photo.png", kind: "image", subtitle: "Pictures" },
      { title: "clip.mp4", path: "C:\\Users\\jayla\\Videos\\clip.mp4", kind: "video", subtitle: "Videos" },
      { title: "track.mp3", path: "C:\\Users\\jayla\\Music\\track.mp3", kind: "audio", subtitle: "Music" },
      { title: "archive.zip", path: "C:\\Users\\jayla\\Downloads\\archive.zip", kind: "archive", subtitle: "Downloads" },
      { title: "25 × 42", subtitle: "1050", action: "calc", kind: "unknown" },
    ];
    const demoIcon =
      "data:image/svg+xml," +
      encodeURIComponent(
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32"><rect width="32" height="32" rx="8" fill="#7aa2ff"/><circle cx="16" cy="16" r="9" fill="#fff"/></svg>'
      );
    sample.forEach((x) => {
      if (x.kind === "application") x.icon = demoIcon;
    });
    const habits = [
      { title: "firefox", action: "habit", subtitle: "Typed often" },
      { title: "notes", action: "habit", subtitle: "Typed often" },
    ];
    const needle = q.trim().toLowerCase();
    const minis = [
      { title: "72°F · Clear", subtitle: "Local weather · enter copies", kind: "weather", category: "mini", meter: 0, action: "copy" },
      { title: "10:42:00 AM", subtitle: "Friday, September 25, 2026", kind: "time", category: "mini", action: "copy" },
      { title: "C:\\  41% used  ·  412 GB free of 931 GB", subtitle: "Windows", kind: "disk", category: "mini", meter: 41, action: "copy" },
      { title: "RAM  62%  ·  19.8 GB used of 32.0 GB", subtitle: "Physical memory", kind: "ram", category: "mini", meter: 62, action: "copy" },
      { title: "CPU  18%", subtitle: "Processor load", kind: "cpu", category: "mini", meter: 18, action: "copy" },
      { title: "Code.exe  ·  CPU 4.2%  ·  RAM 612 MB", subtitle: "PID 4412  ·  42 threads", kind: "process", category: "mini", meter: 4, action: "copy" },
    ];
    const macros = [
      { title: "!yt cats", subtitle: "https://www.youtube.com/results?search_query=cats", kind: "macro", category: "macro", action: "web" },
    ];
    const clip = {
      title: "path/to/notes.md",
      subtitle: "Clipboard · notes about ranking",
      kind: "clipboard",
      category: "clipboard",
      action: "copy",
    };
    const matched = needle
      ? sample.filter((x) => ((x.title || "") + (x.path || "")).toLowerCase().includes(needle))
      : sample;
    const miniHit =
      needle === "weather" ||
      needle === "time" ||
      needle === "disk" ||
      needle === "disku" ||
      needle === "ram" ||
      needle === "cpu" ||
      needle.startsWith("process")
        ? minis.filter((m) => (m.kind || "").includes(needle.split(" ")[0]) || needle.startsWith("process"))
        : needle === "yt cats" || needle.startsWith("!yt")
          ? macros
          : needle === "clip" || needle === "clipboard"
            ? [clip]
            : [];
    const typed = needle
      ? habits.filter((h) => h.title.includes(needle) && h.title !== needle)
      : habits;
    const next = typed.concat(miniHit).concat(needle && miniHit.length ? [] : matched);
    setTimeout(() => {
      if (id !== seq) return;
      onNative({ type: "results", items: next });
    }, 40);
  }

  nativeSend({ type: "ready" });
  if (params.has("preview")) show();
})();
