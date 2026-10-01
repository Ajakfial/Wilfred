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
  nativeSend({
    type: "resize",
    width: Math.ceil(shell.getBoundingClientRect().width),
    height: Math.ceil(shell.getBoundingClientRect().height),
  });
}

export const badges: Record<string, string> = {
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
  bookmark: "bi-bookmark",
  history: "bi-clock-history",
  tab: "bi-window",
  calc: "bi-calculator",
  convert: "bi-rulers",
  web: "bi-search",
  file: "bi-file-earmark",
  clipboard: "bi-clipboard",
  clips: "bi-clipboard2-data",
  macro: "bi-lightning",
  mini: "bi-stars",
  weather: "bi-cloud-sun",
  time: "bi-clock",
  disk: "bi-device-hdd",
  disku: "bi-device-hdd",
  ram: "bi-memory",
  cpu: "bi-cpu",
  process: "bi-activity",
  window: "bi-window-stack",
  battery: "bi-battery-half",
  host: "bi-pc",
  ip: "bi-ethernet",
  uptime: "bi-hourglass-split",
  user: "bi-person",
  clip: "bi-clipboard-check",
  snippet: "bi-card-text",
  plugin: "bi-puzzle",
  os: "bi-windows",
  cores: "bi-cpu-fill",
  screen: "bi-display",
  swap: "bi-layers",
  help: "bi-question-circle",
  speedtest: "bi-speedometer2",
  screenshot: "bi-camera",
  content: "bi-file-text",
  semantic: "bi-stars",
  emoji: "bi-emoji-smile",
  symbol: "bi-asterisk",
  fx: "bi-currency-exchange",
  tz: "bi-globe2",
  color: "bi-palette",
  uuid: "bi-key",
  base64: "bi-type",
  sha256: "bi-hash",
  lorem: "bi-text-paragraph",
  json: "bi-code-square",
  system: "bi-power",
  lock: "bi-lock",
  sleep: "bi-moon",
  shutdown: "bi-power",
  restart: "bi-arrow-repeat",
  logout: "bi-box-arrow-right",
  empty_trash: "bi-trash",
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
  system: "sys",
  lock: "sys",
  sleep: "sys",
  shutdown: "sys",
  restart: "sys",
  logout: "sys",
  empty_trash: "sys",
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

/** Split text into plain/highlighted runs for <mark> rendering. */
export function highlightRuns(
  text: string,
  needle: string,
): Array<{ text: string; mark: boolean }> {
  if (!text || !needle) return text ? [{ text, mark: false }] : [];
  const lower = text.toLowerCase();
  const n = needle.toLowerCase();
  let pos = lower.indexOf(n);
  if (pos < 0) return [{ text, mark: false }];
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
