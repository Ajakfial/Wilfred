import { isSpeedtestQuery } from "./protocol";
import type { ResultItem } from "./types";

// Offline demo data shown when the page runs without a native host
// (plain browser preview). Mirrors the behavior of the native backend
// closely enough to exercise the UI, including typo correction + ghost.

interface DemoEnv {
  isWin: boolean;
  isMac: boolean;
  sep: string;
  home: string;
  join: (...parts: string[]) => string;
  osName: string;
  disk: string;
  procName: string;
  apps: Array<{ title: string; path: string; subtitle: string }>;
}

function demoConvert(q: string): ResultItem | null {
  const raw = q.trim();
  const m = raw.match(
    /^(?:convert\s+)?(-?\d+(?:\.\d+)?)\s*([a-z°µμ/%0-9]+)\s+(?:to|in)\s+([a-z°µμ/%0-9]+)\s*$/i,
  );
  if (!m) return null;
  const value = Number(m[1]);
  const from = m[2].toLowerCase().replace(/[\s.\-]/g, "");
  const to = m[3].toLowerCase().replace(/[\s.\-]/g, "");
  const table: Record<string, [string, number, number?]> = {
    nm: ["L", 1e-9],
    um: ["L", 1e-6],
    mm: ["L", 1e-3],
    cm: ["L", 0.01],
    dm: ["L", 0.1],
    m: ["L", 1],
    km: ["L", 1000],
    in: ["L", 0.0254],
    inch: ["L", 0.0254],
    inches: ["L", 0.0254],
    ft: ["L", 0.3048],
    foot: ["L", 0.3048],
    feet: ["L", 0.3048],
    yd: ["L", 0.9144],
    mi: ["L", 1609.344],
    mile: ["L", 1609.344],
    miles: ["L", 1609.344],
    mg: ["M", 1e-6],
    g: ["M", 0.001],
    kg: ["M", 1],
    t: ["M", 1000],
    tonne: ["M", 1000],
    lb: ["M", 0.45359237],
    lbs: ["M", 0.45359237],
    oz: ["M", 0.028349523125],
    ml: ["V", 1e-6],
    l: ["V", 0.001],
    liter: ["V", 0.001],
    liters: ["V", 0.001],
    litre: ["V", 0.001],
    m3: ["V", 1],
    gal: ["V", 0.003785411784],
    m2: ["A", 1],
    km2: ["A", 1e6],
    ha: ["A", 1e4],
    acre: ["A", 4046.8564224],
    c: ["T", 1, 273.15],
    celsius: ["T", 1, 273.15],
    f: ["T", 5 / 9, 273.15 - (32 * 5) / 9],
    fahrenheit: ["T", 5 / 9, 273.15 - (32 * 5) / 9],
    k: ["T", 1, 0],
    kelvin: ["T", 1, 0],
    "m/s": ["S", 1],
    "km/h": ["S", 1000 / 3600],
    kph: ["S", 1000 / 3600],
    mph: ["S", 1609.344 / 3600],
    j: ["E", 1],
    kj: ["E", 1000],
    cal: ["E", 4.184],
    kcal: ["E", 4184],
    pa: ["P", 1],
    kpa: ["P", 1000],
    bar: ["P", 1e5],
    atm: ["P", 101325],
    psi: ["P", 6894.757293168],
    s: ["t", 1],
    min: ["t", 60],
    h: ["t", 3600],
    hour: ["t", 3600],
    kb: ["D", 1024],
    mb: ["D", 1024 ** 2],
    gb: ["D", 1024 ** 3],
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

/** Browser-only calculator. The input is whitelisted to digits and operators
 *  before evaluation, so nothing but arithmetic can run. */
function demoCalc(q: string): ResultItem | null {
  const raw = q.trim();
  if (!/^[\d\s.,+\-*/×÷x()%^]+$/.test(raw)) return null;
  if (!/[+\-*/×÷x^%]/.test(raw.replace(/^\s*-/, ""))) return null;
  const expr = raw.replace(/[×x]/g, "*").replace(/÷/g, "/").replace(/\^/g, "**").replace(/,/g, "");
  try {
    const v = Function(`"use strict";return (${expr})`)() as unknown;
    if (typeof v !== "number" || !Number.isFinite(v)) return null;
    const out = Number(v.toPrecision(12)).toLocaleString("en-US", { maximumFractionDigits: 10 });
    return {
      title: out,
      subtitle: `Calculator · ${raw}`,
      action: "calc",
      kind: "unknown",
      actions: [{ id: "copy_text", label: "Copy" }],
    };
  } catch {
    return null;
  }
}

function demoEnv(): DemoEnv {
  const nav = navigator as Navigator & { userAgentData?: { platform?: string } };
  const plat = nav.userAgentData?.platform || navigator.platform || navigator.userAgent || "";
  const isMac = /mac/i.test(plat);
  const isWin = !isMac && /win/i.test(plat);
  const sep = isWin ? "\\" : "/";
  const home = isWin ? "%USERPROFILE%" : "~";
  const join = (...parts: string[]) => parts.join(sep);
  return {
    isWin,
    isMac,
    sep,
    home,
    join,
    osName: isWin ? "Windows" : isMac ? "macOS" : "Linux",
    disk: isWin ? "C:\\" : "/",
    procName: isWin ? "Code.exe" : "Code",
    apps: isWin
      ? [
          { title: "Firefox", path: "C:\\Program Files\\Firefox\\firefox.exe", subtitle: "C:\\Program Files\\Firefox" },
          { title: "File Explorer", path: "C:\\Windows\\explorer.exe", subtitle: "C:\\Windows" },
          { title: "Visual Studio Code", path: "%LOCALAPPDATA%\\Programs\\Microsoft VS Code\\Code.exe", subtitle: "Microsoft VS Code" },
        ]
      : isMac
        ? [
            { title: "Firefox", path: "/Applications/Firefox.app", subtitle: "/Applications" },
            { title: "Finder", path: "/System/Library/CoreServices/Finder.app", subtitle: "/System/Library/CoreServices" },
            { title: "Visual Studio Code", path: "/Applications/Visual Studio Code.app", subtitle: "Visual Studio Code" },
          ]
        : [
            { title: "Firefox", path: "/usr/bin/firefox", subtitle: "/usr/bin" },
            { title: "Files", path: "/usr/bin/nautilus", subtitle: "/usr/bin" },
            { title: "Visual Studio Code", path: "/usr/bin/code", subtitle: "Visual Studio Code" },
          ],
  };
}

function speedtestRerun(q: string): boolean {
  return /\b(again|retry|new|rerun)$/.test((q || "").trim().toLowerCase());
}

let demoSpeedAt = 0;
let demoSpeedKey = "";

export function demoQuery(q: string): ResultItem[] {
  const conv = demoConvert(q);
  if (conv) return [conv];
  const calc = demoCalc(q);
  if (calc) return [calc];
  const needle = (q || "").trim().toLowerCase();
  if (isSpeedtestQuery(needle)) {
    if (!demoSpeedAt || (speedtestRerun(needle) && demoSpeedKey !== needle)) {
      demoSpeedAt = Date.now();
    }
    demoSpeedKey = needle;
    const elapsed = (Date.now() - demoSpeedAt) / 1000;
    let down = 0;
    let up = 0;
    let ping = 0;
    let phase = "Measuring ping…";
    let meter = 8;
    if (elapsed < 0.8) {
      ping = Math.max(1, elapsed * 18);
      phase = "Measuring ping…";
    } else if (elapsed < 6) {
      ping = 14;
      down = Math.min(240, 12 + (elapsed - 0.8) * 38);
      phase = "Downloading… live";
      meter = 12 + Math.min(48, ((elapsed - 0.8) / 5.2) * 48);
    } else if (elapsed < 10) {
      ping = 14;
      down = 186;
      up = Math.min(42, 4 + (elapsed - 6) * 9);
      phase = "Uploading… live";
      meter = 60 + Math.min(35, ((elapsed - 6) / 4) * 35);
    } else {
      ping = 14;
      down = 186;
      up = 38;
      phase = "Done · enter copies";
      meter = 100;
    }
    const fmt = (v: number) =>
      v <= 0 ? "—" : v < 10 ? v.toFixed(2) + " Mbps" : v.toFixed(1) + " Mbps";
    const pingTxt = ping ? Math.round(ping) + " ms" : "—";
    const copy = `Download ${fmt(down)} · Upload ${fmt(up)} · Ping ${pingTxt}`;
    return [
      {
        title: `↓ ${fmt(down)}   ↑ ${fmt(up)}`,
        subtitle: `Ping ${pingTxt} · ${phase}`,
        kind: "speedtest",
        category: "mini",
        meter,
        action: "copy",
        payload: copy,
      },
      {
        title: `Download  ${fmt(down)}`,
        subtitle: elapsed < 0.8 ? "Waiting…" : elapsed < 6 ? "Live receive" : "Peak throughput",
        kind: "speedtest",
        category: "mini",
        meter: elapsed >= 0.8 && elapsed < 6 ? meter : down > 0 ? 100 : 0,
        action: "copy",
        payload: copy,
      },
      {
        title: `Upload  ${fmt(up)}`,
        subtitle: elapsed < 6 ? "Waiting…" : elapsed < 10 ? "Live send" : "Peak throughput",
        kind: "speedtest",
        category: "mini",
        meter: elapsed >= 6 && elapsed < 10 ? meter : up > 0 ? 100 : 0,
        action: "copy",
        payload: copy,
      },
    ];
  }
  demoSpeedAt = 0;
  demoSpeedKey = "";
  const env = demoEnv();
  const H = env.home;
  const J = env.join;
  const demoIcon =
    "data:image/svg+xml," +
    encodeURIComponent(
      '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32"><rect width="32" height="32" rx="8" fill="#7aa2ff"/><circle cx="16" cy="16" r="9" fill="#fff"/></svg>',
    );
  const sample: ResultItem[] = [
    ...env.apps.map((a) => ({ ...a, kind: "application" as const })),
    { title: "Projects", path: J(H, "Projects"), kind: "directory", subtitle: H },
    { title: "Downloads", path: J(H, "Downloads"), kind: "directory", subtitle: H },
    { title: "notes.md", path: J(H, "Documents", "notes.md"), kind: "document", subtitle: "Documents" },
    { title: "readme.md", path: J(H, "Projects", "Wilfred", "README.md"), kind: "document", subtitle: "Wilfred" },
    { title: "engine.cpp", path: J(H, "Projects", "Wilfred", "src", "search", "engine.cpp"), kind: "source", subtitle: "search" },
    { title: "photo.png", path: J(H, "Pictures", "photo.png"), kind: "image", subtitle: "Pictures" },
    { title: "clip.mp4", path: J(H, "Videos", "clip.mp4"), kind: "video", subtitle: "Videos" },
    { title: "track.mp3", path: J(H, "Music", "track.mp3"), kind: "audio", subtitle: "Music" },
    { title: "archive.zip", path: J(H, "Downloads", "archive.zip"), kind: "archive", subtitle: "Downloads" },
    { title: "1,050", subtitle: "Calculator · 25 × 42", action: "calc", kind: "unknown" },
  ];
  sample.forEach((x) => {
    if (x.kind === "application") x.icon = demoIcon;
    if (x.action === "calc") {
      x.actions = [{ id: "copy_text", label: "Copy" }];
      return;
    }
    x.actions = [
      { id: "open", label: "Open" },
      { id: "reveal", label: "Show in folder" },
      { id: "copy_path", label: "Copy path" },
    ];
  });
  const habits: ResultItem[] = [
    { title: "firefox", action: "habit", subtitle: "Typed often" },
    { title: "notes", action: "habit", subtitle: "Typed often" },
  ];
  const minis: ResultItem[] = [
    { title: "72°F · Clear", subtitle: "Local weather · enter copies", kind: "weather", category: "mini", meter: 0, action: "copy" },
    { title: "10:42:00 AM", subtitle: "Friday, September 25, 2026", kind: "time", category: "mini", action: "copy" },
    { title: `${env.disk}  41% used  ·  412 GB free of 931 GB`, subtitle: env.osName, kind: "disk", category: "mini", meter: 41, action: "copy" },
    { title: "RAM  62%  ·  19.8 GB used of 32.0 GB", subtitle: "Physical memory", kind: "ram", category: "mini", meter: 62, action: "copy" },
    { title: "CPU  18%", subtitle: "Processor load", kind: "cpu", category: "mini", meter: 18, action: "copy" },
    { title: `${env.procName}  ·  CPU 4.2%  ·  RAM 612 MB`, subtitle: "PID 4412  ·  42 threads", kind: "process", category: "mini", meter: 4, action: "copy" },
  ];
  const toggles: ResultItem[] = [
    {
      title: "Wi-Fi: On · DemoNet",
      subtitle: "wifi on | wifi off | wifi toggle — enter toggles",
      kind: "wifi",
      category: "toggle",
      payload: "toggle:wifi:toggle",
      action: "mini",
      actions: [
        { id: "open", label: "Apply" },
        { id: "copy_text", label: "Copy status" },
      ],
    },
    {
      title: "Bluetooth: On",
      subtitle: "bluetooth on | bluetooth off — enter toggles",
      kind: "bluetooth",
      category: "toggle",
      payload: "toggle:bluetooth:toggle",
      action: "mini",
      actions: [
        { id: "open", label: "Apply" },
        { id: "copy_text", label: "Copy status" },
      ],
    },
    {
      title: "Volume: 50%",
      subtitle: "volume 50 | volume mute | volume up — enter toggles mute",
      kind: "volume",
      category: "toggle",
      payload: "toggle:volume:mute_toggle",
      meter: 50,
      action: "mini",
      actions: [
        { id: "open", label: "Apply" },
        { id: "copy_text", label: "Copy status" },
      ],
    },
    {
      title: "Brightness: 75%",
      subtitle: "brightness 70 | brightness up — enter applies",
      kind: "brightness",
      category: "toggle",
      payload: "toggle:brightness:set:75",
      meter: 75,
      action: "mini",
      actions: [
        { id: "open", label: "Apply" },
        { id: "copy_text", label: "Copy status" },
      ],
    },
  ];
  const settingsDemo: ResultItem[] = [
    {
      title: "Wi-Fi settings",
      subtitle: "settings wifi — enter opens",
      kind: "settings",
      category: "settings",
      payload: "settings:wifi",
      action: "mini",
      actions: [{ id: "open", label: "Open settings" }],
    },
    {
      title: "Sound settings",
      subtitle: "settings sound — enter opens",
      kind: "settings",
      category: "settings",
      payload: "settings:sound",
      action: "mini",
      actions: [{ id: "open", label: "Open settings" }],
    },
    {
      title: "Settings",
      subtitle: "Main settings — enter opens",
      kind: "settings",
      category: "settings",
      payload: "settings:",
      action: "mini",
      actions: [{ id: "open", label: "Open settings" }],
    },
  ];
  const setupDemo: ResultItem[] = [
    {
      title: "Setup: ready (roots, hotkey, browser ok)",
      subtitle: "setup status — enter opens wilfred.yml",
      kind: "setup",
      category: "setup",
      payload: "config:open",
      action: "mini",
      actions: [{ id: "open", label: "Open" }],
    },
    {
      title: "Validate wilfred.yml",
      subtitle: "setup validate — enter checks now",
      kind: "setup",
      category: "setup",
      payload: "config:validate",
      action: "mini",
      actions: [{ id: "open", label: "Open" }],
    },
    {
      title: "Edit wilfred.yml",
      subtitle: "config open — enter opens in editor",
      kind: "config",
      category: "config",
      payload: "config:open",
      action: "mini",
      actions: [{ id: "open", label: "Open" }],
    },
  ];
  const remotesDemo: ResultItem[] = [
    {
      title: "Demo wiki hit",
      subtitle: "Demo result · wiki",
      kind: "remote",
      category: "remote",
      path: "https://example.com/wiki/demo",
      payload: "https://example.com/wiki/demo",
      action: "web",
      actions: [
        { id: "open", label: "Open" },
        { id: "copy_text", label: "Copy" },
      ],
    },
  ];
  const pkgsDemo: ResultItem[] = [
    {
      title: "Mozilla Firefox",
      subtitle: "Mozilla.Firefox · 131.0 · via winget — Enter installs",
      kind: "pkg",
      category: "pkg",
      path: "pkg-install:winget:Mozilla.Firefox",
      payload: "winget install --exact --id Mozilla.Firefox",
      action: "mini",
      actions: [
        { id: "open", label: "Install" },
        { id: "copy_text", label: "Copy install command" },
      ],
    },
  ];
  const pkgManagersDemo: ResultItem[] = [
    {
      title: "winget — available",
      subtitle: "e.g. `winget firefox` — Enter copies an example",
      kind: "pkg",
      category: "pkg",
      payload: "winget firefox",
      action: "mini",
      actions: [{ id: "copy_text", label: "Copy example" }],
    },
    {
      title: "brew — available",
      subtitle: "e.g. `brew firefox` — Enter copies an example",
      kind: "pkg",
      category: "pkg",
      payload: "brew firefox",
      action: "mini",
      actions: [{ id: "copy_text", label: "Copy example" }],
    },
  ];
  const macros: ResultItem[] = [
    { title: "!yt cats", subtitle: "https://www.youtube.com/results?search_query=cats", kind: "macro", category: "macro", action: "web" },
  ];
  const clip: ResultItem = {
    title: "path/to/notes.md",
    subtitle: "Clipboard · notes about ranking",
    kind: "clipboard",
    category: "clipboard",
    action: "copy",
  };
  const snippets: ResultItem[] = [
    {
      title: "sig",
      subtitle: "Snippet · sig · enter pastes",
      kind: "snippet",
      category: "snippet",
      action: "expand",
      payload: "Best regards,\nYour Name",
      actions: [
        { id: "paste", label: "Paste" },
        { id: "copy_text", label: "Copy text" },
      ],
    },
  ];
  const plugins: ResultItem[] = [
    {
      title: "Ping host",
      subtitle: "Plugin · demo",
      kind: "plugin",
      category: "plugin",
      action: "plugin",
      path: "8.8.8.8",
      actions: [
        { id: "open", label: "Run" },
        { id: "copy_path", label: "Copy path" },
      ],
    },
  ];
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
      : needle === "wifi" || needle === "wlan" || needle === "wireless" || needle.startsWith("wifi ") || needle.startsWith("wlan ")
        ? toggles.filter((m) => m.kind === "wifi")
        : needle === "bluetooth" || needle === "bt" || needle.startsWith("bluetooth ") || needle.startsWith("bt ")
          ? toggles.filter((m) => m.kind === "bluetooth")
          : needle === "volume" || needle === "vol" || needle.startsWith("volume ") || needle.startsWith("vol ")
            ? toggles.filter((m) => m.kind === "volume")
            : needle === "brightness" || needle === "bright" || needle === "backlight" || needle === "dim" || needle.startsWith("brightness ")
              ? toggles.filter((m) => m.kind === "brightness")
              : needle === "settings" || needle === "setting" || needle === "prefs" || needle.startsWith("settings ")
                ? settingsDemo.filter((m) =>
                    needle === "settings" || needle === "setting" || needle === "prefs"
                      ? true
                      : (m.title || "").toLowerCase().includes(needle.split(" ").slice(1).join(" ")),
                  )
                : needle === "setup" || needle === "wizard" || needle === "onboard" || needle.startsWith("setup ")
                  ? setupDemo.filter((m) => m.category === "setup")
                  : needle === "config" || needle === "configuration" || needle.startsWith("config ")
                    ? setupDemo.filter((m) => m.category === "config" || m.category === "setup")
                    : needle === "remote" || needle.startsWith("remote ")
                      ? remotesDemo
                      : needle === "toggle" || needle.startsWith("toggle ")
                        ? toggles
                        : needle === "winget" || needle.startsWith("winget ") ||
                            needle === "brew" || needle.startsWith("brew ") ||
                            needle === "apt" || needle.startsWith("apt ") ||
                            needle.startsWith("pkg ")
                          ? pkgsDemo
                          : needle === "packages" || needle === "package" || needle === "pkg" ||
                              needle.startsWith("packages ") || needle.startsWith("package ")
                            ? pkgManagersDemo
                        : needle === "yt cats" || needle.startsWith("!yt")
                          ? macros
                          : needle === "clip" || needle === "clipboard"
                            ? [clip]
                            : needle.startsWith(";") || needle.startsWith("snip")
                              ? snippets
                              : needle === "plugin" || needle === "ping"
                                ? plugins
                                : [];
  const typed = needle
    ? habits.filter((h) => (h.title || "").includes(needle) && h.title !== needle)
    : habits;
  return typed.concat(miniHit).concat(needle && miniHit.length ? [] : matched);
}

function damerauDemo(a: string, b: string, maxDist: number): number {
  if (a === b) return 0;
  const n = a.length;
  const m = b.length;
  if (Math.abs(n - m) > maxDist) return maxDist + 1;
  const d: number[][] = Array.from({ length: n + 1 }, (_, i) => [i, ...Array(m).fill(0)]);
  for (let j = 0; j <= m; j++) d[0][j] = j;
  for (let i = 1; i <= n; i++) {
    for (let j = 1; j <= m; j++) {
      const cost = a[i - 1] === b[j - 1] ? 0 : 1;
      let v = Math.min(d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + cost);
      if (i > 1 && j > 1 && a[i - 1] === b[j - 2] && a[i - 2] === b[j - 1]) v = Math.min(v, d[i - 2][j - 2] + 1);
      d[i][j] = v;
    }
  }
  return d[n][m] <= maxDist ? d[n][m] : maxDist + 1;
}

const demoVocab = [
  "weather", "time", "disk", "ram", "cpu", "process", "battery", "clip", "clips",
  "speedtest", "screenshot", "emoji", "symbol", "color", "uuid", "base64", "sha256",
  "lorem", "json", "lock", "sleep", "shutdown", "restart", "logout", "empty trash",
  "wifi", "bluetooth", "volume", "brightness", "settings", "setup", "config",
  "remote", "toggle",
  "winget", "brew", "apt", "packages", "choco", "flatpak", "pacman",
  "firefox", "notes", "finder", "files", "visual studio code",
];

export interface DemoAssist {
  correction: string;
  ghost: string;
  candidates: string[];
}

/** Browser-only assist mirroring build_assist (vocab + typo-prefix). */
export function demoAssist(q: string): DemoAssist {
  const out: DemoAssist = { correction: "", ghost: "", candidates: [] };
  const raw = (q || "").trim();
  if (!raw || /\s$/.test(q)) return out;
  const n = raw.toLowerCase();
  // Candidates: prefix first, then typo-prefix.
  for (const w of demoVocab) {
    if (w.startsWith(n) && w !== n) {
      if (!out.candidates.includes(w)) out.candidates.push(w);
      if (out.candidates.length >= 6) break;
    }
  }
  if (out.candidates.length < 6 && n.length >= 3) {
    for (const w of demoVocab) {
      if (out.candidates.length >= 6) break;
      if (w.length < n.length || w.startsWith(n)) continue;
      const head = w.slice(0, n.length);
      if (head === n) continue;
      const thr = n.length < 5 ? 1 : 2;
      if (damerauDemo(n, head, thr) <= thr && (n[0] === head[0] || damerauDemo(n, head, thr) === 1)) {
        if (!out.candidates.includes(w)) out.candidates.push(w);
      }
    }
  }
  // Correction: closest vocab within distance.
  let best = "";
  let bestD = 99;
  const thrAll = n.length < 5 ? 1 : 2;
  for (const w of demoVocab) {
    if (w === n || w.includes(" ")) continue;
    if (Math.abs(w.length - n.length) > thrAll) continue;
    const d = damerauDemo(n, w, thrAll);
    if (d <= thrAll && d < bestD) {
      bestD = d;
      best = w;
    }
  }
  if (best && (bestD === 1 || (bestD === 2 && n.length >= 4 && n[0] === best[0]))) out.correction = best;
  // Ghost: first extending candidate.
  for (const c of out.candidates) {
    if (c.length > raw.length && c.toLowerCase().startsWith(n)) {
      // Preserve typed casing for the typed portion.
      out.ghost = raw + c.slice(raw.length);
      break;
    }
  }
  if (!out.ghost && out.candidates.length) out.ghost = out.candidates[0];
  if (out.correction && out.correction.toLowerCase() === n) out.correction = "";
  return out;
}
