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

export function reportSize(): void {
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
};

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

/** Selectable rows (habit rows act as input shortcuts, except mini ones). */
export function rowsOf(items: ResultItem[]): RowEntry[] {
  const rows: RowEntry[] = [];
  items.forEach((item, index) => {
    if (item.action !== "habit" || item.category === "mini") rows.push({ item, index });
  });
  return rows;
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
