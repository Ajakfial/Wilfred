import type { ActionItem, ResultItem, RowEntry } from "./types";

/** True when running without a native host (opened directly in a browser). */
export function hasNativeHost(): boolean {
  return Boolean(
    (window as unknown as { chrome?: { webview?: unknown } }).chrome?.webview ||
      (window as unknown as { webkit?: { messageHandlers?: { wilfred?: unknown } } }).webkit
        ?.messageHandlers?.wilfred,
  );
}

export function nativeSend(msg: object): void {
  const w = window as unknown as {
    chrome?: { webview?: { postMessage: (o: object) => void } };
    webkit?: { messageHandlers?: { wilfred?: { postMessage: (s: string) => void } } };
  };
  if (w.chrome?.webview) {
    w.chrome.webview.postMessage(msg);
    return;
  }
  if (w.webkit?.messageHandlers?.wilfred) {
    w.webkit.messageHandlers.wilfred.postMessage(JSON.stringify(msg));
  }
}

/**
 * Auto-size vs. user-size.
 *
 * By default the window hugs the content: the UI measures `.shell` and tells
 * the native host (`resize`) how big to be. When the user starts dragging a
 * window edge, the native host sends `{type:"fit"}` (it knows for certain; the
 * UI must not guess from viewport sizes). We then switch to `html.fit` (the
 * panel fills the viewport and the lists flex/scroll inside it) and stop
 * sending `resize`, so the host never fights the user's chosen size.
 */
let fitMode = false;

export function isFitMode(): boolean {
  return fitMode;
}

export function enterFitMode(): void {
  if (fitMode) return;
  fitMode = true;
  document.documentElement.classList.add("fit");
}

export function reportSize(): void {
  if (fitMode) return;
  const shell = document.querySelector(".shell");
  if (!shell) return;
  const r = shell.getBoundingClientRect();
  nativeSend({
    type: "resize",
    width: Math.ceil(r.width),
    height: Math.ceil(r.height),
  });
}

/** Current platform for placeholder/hints. Same bundle on Win/Mac/Linux. */
export function platformId(): "win" | "mac" | "linux" {
  const nav = navigator as Navigator & { userAgentData?: { platform?: string } };
  const p = (nav.userAgentData?.platform || navigator.platform || navigator.userAgent || "").toLowerCase();
  if (p.includes("mac")) return "mac";
  if (p.includes("win")) return "win";
  return "linux";
}

export function hotkeyHint(): string {
  return platformId() === "mac" ? "⌘⌥W" : "Ctrl Alt W";
}

/** Maps backend kinds to icon names in src/components/icons.tsx (Lucide-style inline SVGs). */
export const badges: Record<string, string> = {
  application: "app",
  executable: "play",
  directory: "folder",
  document: "fileText",
  image: "image",
  video: "video",
  audio: "music",
  archive: "archive",
  source: "code",
  config: "settings",
  shortcut: "external",
  browser: "globe",
  bookmark: "bookmark",
  history: "history",
  tab: "appWindow",
  calc: "calculator",
  convert: "convert",
  web: "globe",
  file: "file",
  clipboard: "clipboardList",
  clips: "clipboardList",
  macro: "zap",
  mini: "sparkles",
  weather: "cloudSun",
  time: "clock",
  disk: "hardDrive",
  disku: "hardDrive",
  ram: "memoryStick",
  cpu: "cpu",
  process: "activity",
  window: "appWindow",
  battery: "battery",
  host: "monitor",
  ip: "network",
  uptime: "timer",
  user: "user",
  clip: "clipboardCheck",
  snippet: "note",
  plugin: "puzzle",
  os: "monitor",
  cores: "cpu",
  screen: "monitor",
  swap: "clipboardList",
  help: "help",
  speedtest: "gauge",
  screenshot: "camera",
  content: "fileSearch",
  semantic: "sparkles",
  ai: "sparkles",
  event: "clock",
  contact: "user",
  note: "note",
  calendar: "clock",
  emoji: "smile",
  symbol: "asterisk",
  fx: "convert",
  tz: "clock",
  color: "palette",
  uuid: "key",
  base64: "type",
  sha256: "hash",
  lorem: "text",
  json: "braces",
  system: "power",
  lock: "lock",
  sleep: "moon",
  shutdown: "power",
  restart: "restart",
  logout: "logout",
  empty_trash: "trash",
};

export const groups: Record<string, string> = {
  application: "app",
  executable: "app",
  directory: "folder",
  document: "doc",
  content: "doc",
  file: "doc",
  archive: "doc",
  image: "media",
  video: "media",
  audio: "media",
  source: "code",
  config: "code",
  calc: "calc",
  convert: "calc",
  fx: "calc",
  tz: "calc",
  color: "calc",
  web: "web",
  browser: "web",
  macro: "web",
  bookmark: "web",
  history: "web",
  tab: "web",
  speedtest: "speed",
  screenshot: "media",
  semantic: "mini",
  ai: "mini",
  calendar: "mini",
  contact: "mini",
  note: "mini",
  clipboard: "clip",
  clips: "clip",
  clip: "clip",
  snippet: "snippet",
  plugin: "plugin",
  system: "sys",
  lock: "sys",
  sleep: "sys",
  shutdown: "sys",
  restart: "sys",
  logout: "sys",
  empty_trash: "sys",
};

export const groupLabels: Record<string, string> = {
  app: "Apps",
  folder: "Folders",
  doc: "Files",
  media: "Media",
  code: "Code",
  calc: "Answers",
  web: "Web",
  speed: "Network",
  sys: "System",
  mini: "Insights",
  clip: "Clipboard",
  snippet: "Snippets",
  plugin: "Plugins",
};

/** Human labels for the right-hand accessory on a row. */
const kindLabels: Record<string, string> = {
  application: "Application",
  executable: "Executable",
  directory: "Folder",
  document: "Document",
  source: "Source",
  config: "Config",
  shortcut: "Shortcut",
  clipboard: "Clipboard",
  clips: "Clipboard",
  clip: "Clipboard",
  content: "Text match",
  semantic: "Smart match",
  ai: "Smart match",
  web: "Web",
  macro: "Web shortcut",
  disku: "Disk usage",
  ram: "Memory",
  cpu: "Processor",
  tz: "Time zone",
  fx: "Currency",
};

export function kindLabel(k: string): string {
  if (!k || k === "habit") return "";
  if (kindLabels[k]) return kindLabels[k];
  const t = k.replace(/_/g, " ");
  return t.charAt(0).toUpperCase() + t.slice(1);
}

export const tips = ["25 * 42", "weather", "speedtest", "type:image", "!yt cats", "clip"];

export function kindOf(item: ResultItem): string {
  if (item.action === "expand" || item.category === "snippet")
    return item.kind === "clip" ? "clip" : "snippet";
  if (item.action === "plugin" || item.category === "plugin") return "plugin";
  if (item.action === "calc") return "calc";
  if (item.action === "convert") {
    if (item.kind && badges[item.kind]) return item.kind;
    return "convert";
  }
  if (item.action === "web") return "web";
  if (item.category === "system") return item.kind || "system";
  if (item.action === "habit") return "habit";
  if (item.category === "clipboard" || item.kind === "clipboard") return "clipboard";
  if (item.category === "macro" || item.kind === "macro") return "macro";
  if (item.category === "mini") return item.kind || "mini";
  if (item.category === "content") return "content";
  return item.kind || "file";
}

export function actionsOf(item: ResultItem | undefined): ActionItem[] {
  return item && Array.isArray(item.actions) ? item.actions : [];
}

export function groupOf(item: ResultItem): string {
  const k = kindOf(item);
  return groups[k] || (item.category === "mini" ? "mini" : "doc");
}

/** Selectable rows in *display order*: the backend rank is preserved inside a
 *  section, and sections appear in order of their best-ranked member, so the
 *  top hit is always row 0. Keyboard position == visual position. Habit rows
 *  act as input shortcuts (except mini ones). When the speed-test summary can
 *  be drawn as a card, the redundant per-direction rows are folded into it. */
export function rowsOf(items: ResultItem[]): RowEntry[] {
  const speedCard = items.findIndex((it) => kindOf(it) === "speedtest" && parseSpeed(it) !== null);
  const buckets = new Map<string, RowEntry[]>();
  items.forEach((item, index) => {
    if (item.action === "habit" && item.category !== "mini") return;
    if (speedCard >= 0 && index !== speedCard && kindOf(item) === "speedtest") return;
    const g = groupOf(item);
    const list = buckets.get(g);
    if (list) list.push({ item, index });
    else buckets.set(g, [{ item, index }]);
  });
  const rows: RowEntry[] = [];
  buckets.forEach((list) => rows.push(...list));
  return rows;
}

export interface Section {
  key: string;
  label: string;
  rows: Array<{ entry: RowEntry; pos: number }>;
}

/** Split display-ordered rows into labelled sections. */
export function sectionsOf(rows: RowEntry[]): Section[] {
  const out: Section[] = [];
  rows.forEach((entry, pos) => {
    const key = groupOf(entry.item);
    const last = out[out.length - 1];
    if (last && last.key === key) last.rows.push({ entry, pos });
    else out.push({ key, label: groupLabels[key] || "Results", rows: [{ entry, pos }] });
  });
  return out;
}

export function habitsOf(items: ResultItem[]): ResultItem[] {
  return items.filter((item) => item.action === "habit" && item.category !== "mini");
}

/** Split text into plain/highlighted runs for <mark> rendering.
 *  Substring-first, then subsequence fallback so typo hits ("firefoz" vs
 *  "firefox") still highlight the aligned characters instead of nothing. */
export function highlightRuns(
  text: string,
  needle: string,
): Array<{ text: string; mark: boolean }> {
  if (!text || !needle) return text ? [{ text, mark: false }] : [];
  const lower = text.toLowerCase();
  const n = needle.trim().toLowerCase();
  if (!n) return [{ text, mark: false }];
  let pos = lower.indexOf(n);
  if (pos >= 0) {
    const runs: Array<{ text: string; mark: boolean }> = [];
    let i = 0;
    while (pos >= 0) {
      if (pos > i) runs.push({ text: text.slice(i, pos), mark: false });
      runs.push({ text: text.slice(pos, pos + n.length), mark: true });
      i = pos + n.length;
      pos = lower.indexOf(n, i);
    }
    if (i < text.length) runs.push({ text: text.slice(i), mark: false });
    return runs;
  }
  // Subsequence fallback: highlight matched characters in order.
  let qi = 0;
  const runs: Array<{ text: string; mark: boolean }> = [];
  let buf = "";
  let lastMark = false;
  for (let i = 0; i < text.length && qi < n.length; i++) {
    const isMark = lower[i] === n[qi];
    if (isMark !== lastMark && buf) {
      runs.push({ text: buf, mark: lastMark });
      buf = "";
    }
    lastMark = isMark;
    buf += text[i];
    if (isMark) qi++;
  }
  if (qi < n.length) return [{ text, mark: false }];
  if (buf) runs.push({ text: buf, mark: lastMark });
  // Append remainder unmarked.
  const consumed = runs.reduce((a, r) => a + r.text.length, 0);
  if (consumed < text.length) {
    // runs already covers prefix; append tail.
    return [...runs, { text: text.slice(consumed), mark: false }];
  }
  return runs;
}

/** Ghost suffix after the typed query ("wea" + "ther"). Empty = none. */
export function ghostSuffix(query: string, ghost: string | undefined): string {
  if (!ghost || !query) return "";
  if (ghost.length <= query.length) return "";
  if (ghost.toLowerCase().startsWith(query.toLowerCase())) return ghost.slice(query.length);
  // Typo ghost: show full suggestion as suffix hint (dimmed).
  return "";
}

const speedtestKeys = [
  "speedtest",
  "speed-test",
  "speed_test",
  "netspeed",
  "bandwidth",
  "internetspeed",
];

export function isSpeedtestQuery(q: string): boolean {
  const t = (q || "").trim().toLowerCase();
  for (const k of speedtestKeys) {
    if (t === k) return true;
    if (t.startsWith(k + " ")) {
      const rest = t.slice(k.length).trim();
      return !rest || rest === "again" || rest === "retry" || rest === "new" || rest === "rerun";
    }
  }
  return false;
}

/** Calculator / conversion result: the engine sends the *answer* as title and
 *  "<Label> · <expression>" as subtitle. */
export interface Answer {
  label: string;
  expression: string;
  result: string;
}

export function parseAnswer(item: ResultItem): Answer | null {
  if (item.action !== "calc" && item.action !== "convert") return null;
  const result = (item.title || "").trim();
  if (!result) return null;
  const sub = (item.subtitle || "").trim();
  const cut = sub.indexOf(" · ");
  if (cut > 0) return { label: sub.slice(0, cut), expression: sub.slice(cut + 3), result };
  return { label: item.action === "convert" ? "Conversion" : "Calculator", expression: sub, result };
}

export interface SpeedStat {
  value: string;
  unit: string;
}

export interface Speed {
  down: SpeedStat;
  up: SpeedStat;
  ping: SpeedStat;
  phase: string;
  progress: number;
  done: boolean;
}

function speedStat(raw: string): SpeedStat {
  const t = raw.trim();
  const m = t.match(/^([\d.,]+|—|-)\s*(.*)$/);
  return m ? { value: m[1] === "-" ? "—" : m[1], unit: m[2] } : { value: t || "—", unit: "" };
}

/** Parses the speed-test summary row ("↓ 186 Mbps   ↑ 38 Mbps" +
 *  "Ping 14 ms · phase"). Returns null when the shape is unfamiliar so the
 *  row falls back to a plain list row. */
export function parseSpeed(item: ResultItem): Speed | null {
  if (item.kind !== "speedtest") return null;
  const t = item.title || "";
  const a = t.indexOf("↓");
  const b = t.indexOf("↑");
  if (a < 0 || b < 0 || b < a) return null;
  const down = speedStat(t.slice(a + 1, b));
  const up = speedStat(t.slice(b + 1));
  const sub = item.subtitle || "";
  const pm = sub.match(/Ping\s+([^·]+?)\s*(?:·|$)/i);
  const ping = speedStat(pm ? pm[1] : "—");
  const cut = sub.indexOf(" · ");
  const phase = cut >= 0 ? sub.slice(cut + 3) : "";
  const progress = typeof item.meter === "number" ? Math.max(0, Math.min(100, item.meter)) : 0;
  return { down, up, ping, phase, progress, done: progress >= 100 };
}

/** Footer label for the Enter action on the selected row (sentence case). */
export function primaryLabel(item: ResultItem | undefined): string {
  if (!item) return "Open";
  const k = kindOf(item);
  const first = actionsOf(item)[0]?.label;
  if (item.action === "calc" || item.action === "convert" || k === "speedtest") return "Copy result";
  if (item.action === "expand") return first || "Paste";
  if (item.action === "web") return "Open in browser";
  if (item.action === "copy") return first || "Copy";
  if (item.category === "system") return first || "Run";
  if (k === "application") return "Open application";
  if (k === "directory") return "Open folder";
  if (first && first !== "Open") return first;
  return "Open";
}
