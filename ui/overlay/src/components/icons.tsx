// Wilfred overlay icon library — Lucide-style inline SVG icons.
//
// Replaces the old bootstrap-icons webfont with crisp, dependency-free
// inline SVGs (24x24 grid, round caps/joins, currentColor stroke).
// No font files, no CDN, no FOUT — icons inherit the row tint color
// and stay sharp at any DPI. Style: Lucide (ISC, https://lucide.dev).

import type { JSX } from "react";

export type IconName =
  | "search"
  | "app"
  | "play"
  | "folder"
  | "fileText"
  | "file"
  | "image"
  | "video"
  | "music"
  | "archive"
  | "code"
  | "settings"
  | "external"
  | "globe"
  | "bookmark"
  | "history"
  | "appWindow"
  | "calculator"
  | "convert"
  | "clipboard"
  | "clipboardList"
  | "clipboardCheck"
  | "zap"
  | "sparkles"
  | "cloudSun"
  | "clock"
  | "hardDrive"
  | "memoryStick"
  | "cpu"
  | "activity"
  | "battery"
  | "monitor"
  | "network"
  | "hourglass"
  | "user"
  | "note"
  | "puzzle"
  | "help"
  | "gauge"
  | "camera"
  | "fileSearch"
  | "smile"
  | "asterisk"
  | "palette"
  | "key"
  | "type"
  | "hash"
  | "text"
  | "braces"
  | "power"
  | "lock"
  | "moon"
  | "restart"
  | "logout"
  | "trash"
  | "enter"
  | "more"
  | "chevronRight"
  | "link"
  | "timer"
  | "wand"
  | "arrowRight"
  | "check"
  | "x"
  | "command";

function P(children: JSX.Element | JSX.Element[]): JSX.Element {
  return <>{children}</>;
}

const PATHS: Record<IconName, JSX.Element> = {
  search: P(
    <>
      <circle cx="11" cy="11" r="7" />
      <path d="m21 21-4.35-4.35" />
    </>,
  ),
  app: P(
    <>
      <rect x="3" y="3" width="7" height="7" rx="1.5" />
      <rect x="14" y="3" width="7" height="7" rx="1.5" />
      <rect x="14" y="14" width="7" height="7" rx="1.5" />
      <rect x="3" y="14" width="7" height="7" rx="1.5" />
    </>,
  ),
  play: P(
    <>
      <path d="M6 4.5v15l13-7.5Z" />
    </>,
  ),
  folder: P(
    <>
      <path d="M20 20a2 2 0 0 0 2-2V8a2 2 0 0 0-2-2h-7.9a2 2 0 0 1-1.69-.9L9.6 3.9A2 2 0 0 0 7.93 3H4a2 2 0 0 0-2 2v13a2 2 0 0 0 2 2Z" />
    </>,
  ),
  fileText: P(
    <>
      <path d="M15 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V7Z" />
      <path d="M14 2v4a2 2 0 0 0 2 2h4" />
      <path d="M10 9H8" />
      <path d="M16 13H8" />
      <path d="M16 17H8" />
    </>,
  ),
  file: P(
    <>
      <path d="M15 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V7Z" />
      <path d="M14 2v4a2 2 0 0 0 2 2h4" />
    </>,
  ),
  image: P(
    <>
      <rect x="3" y="3" width="18" height="18" rx="2" />
      <circle cx="9" cy="9" r="2" />
      <path d="m21 15-3.09-3.09a2 2 0 0 0-2.82 0L6 21" />
    </>,
  ),
  video: P(
    <>
      <path d="m22 8-6 4 6 4V8Z" />
      <rect x="2" y="6" width="14" height="12" rx="2" />
    </>,
  ),
  music: P(
    <>
      <path d="M9 18V5l12-2v13" />
      <circle cx="6" cy="18" r="3" />
      <circle cx="18" cy="16" r="3" />
    </>,
  ),
  archive: P(
    <>
      <rect x="2" y="3" width="20" height="5" rx="1" />
      <path d="M4 8v11a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8" />
      <path d="M10 12h4" />
    </>,
  ),
  code: P(
    <>
      <path d="m16 18 6-6-6-6" />
      <path d="m8 6-6 6 6 6" />
    </>,
  ),
  settings: P(
    <>
      <path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 1 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 1 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 1 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 1 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1Z" />
      <circle cx="12" cy="12" r="3" />
    </>,
  ),
  external: P(
    <>
      <path d="M15 3h6v6" />
      <path d="M10 14 21 3" />
      <path d="M18 13v6a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h6" />
    </>,
  ),
  globe: P(
    <>
      <circle cx="12" cy="12" r="10" />
      <path d="M12 2a14.5 14.5 0 0 0 0 20 14.5 14.5 0 0 0 0-20" />
      <path d="M2 12h20" />
    </>,
  ),
  bookmark: P(
    <>
      <path d="m19 21-7-4-7 4V5a2 2 0 0 1 2-2h10a2 2 0 0 1 2 2v16z" />
    </>,
  ),
  history: P(
    <>
      <path d="M3 12a9 9 0 1 0 9-9 9.75 9.75 0 0 0-6.74 2.74L3 8" />
      <path d="M3 3v5h5" />
      <path d="M12 7v5l4 2" />
    </>,
  ),
  appWindow: P(
    <>
      <rect x="2" y="4" width="20" height="16" rx="2" />
      <path d="M2 9h20" />
      <path d="M6 6.5h.01" />
    </>,
  ),
  calculator: P(
    <>
      <rect x="5" y="2" width="14" height="20" rx="2" />
      <path d="M8 6h8" />
      <path d="M8 10h.01" />
      <path d="M12 10h.01" />
      <path d="M16 10h.01" />
      <path d="M8 14h.01" />
      <path d="M12 14h.01" />
      <path d="M16 14h.01" />
      <path d="M8 18h.01" />
      <path d="M12 18h.01" />
      <path d="M16 18h.01" />
    </>,
  ),
  convert: P(
    <>
      <path d="M8 3 4 7l4 4" />
      <path d="M4 7h16" />
      <path d="m16 21 4-4-4-4" />
      <path d="M20 17H4" />
    </>,
  ),
  clipboard: P(
    <>
      <rect x="8" y="2" width="8" height="4" rx="1" />
      <path d="M16 4h2a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2h2" />
    </>,
  ),
  clipboardList: P(
    <>
      <rect x="8" y="2" width="8" height="4" rx="1" />
      <path d="M16 4h2a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2h2" />
      <path d="M12 11h4" />
      <path d="M12 16h4" />
      <path d="M8 11h.01" />
      <path d="M8 16h.01" />
    </>,
  ),
  clipboardCheck: P(
    <>
      <rect x="8" y="2" width="8" height="4" rx="1" />
      <path d="M16 4h2a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2h2" />
      <path d="m9 14 2 2 4-4" />
    </>,
  ),
  zap: P(
    <>
      <path d="M13 2 3 14h9l-1 8 10-12h-9l1-8z" />
    </>,
  ),
  sparkles: P(
    <>
      <path d="M12 3l1.9 5.8 5.8 1.9-5.8 1.9L12 18.4l-1.9-5.8L4.3 10.7l5.8-1.9Z" />
      <path d="M19 3v4" />
      <path d="M21 5h-4" />
    </>,
  ),
  cloudSun: P(
    <>
      <circle cx="7" cy="5" r="2.5" />
      <path d="M7 1.5v.01" />
      <path d="M3.3 3.3l.01.01" />
      <path d="M10.7 3.3l-.01.01" />
      <path d="M17.5 19a4.5 4.5 0 1 0-.42-8.98 6 6 0 0 0-11.7 1.62A4 4 0 0 0 7 19h10.5" />
    </>,
  ),
  clock: P(
    <>
      <circle cx="12" cy="12" r="10" />
      <path d="M12 6v6l4 2" />
    </>,
  ),
  hardDrive: P(
    <>
      <path d="M22 12H2" />
      <path d="M5.45 5.11 2 12v6a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-6l-3.45-6.89A2 2 0 0 0 16.76 4H7.24a2 2 0 0 0-1.79 1.11z" />
      <path d="M6 16h.01" />
      <path d="M10 16h.01" />
    </>,
  ),
  memoryStick: P(
    <>
      <rect x="2" y="8" width="20" height="8" rx="2" />
      <path d="M6 8V5" />
      <path d="M10 8V5" />
      <path d="M14 8V5" />
      <path d="M18 8V5" />
      <path d="M6 16v3" />
      <path d="M10 16v3" />
      <path d="M14 16v3" />
      <path d="M18 16v3" />
    </>,
  ),
  cpu: P(
    <>
      <rect x="4" y="4" width="16" height="16" rx="2" />
      <rect x="9" y="9" width="6" height="6" />
      <path d="M9 1v3" />
      <path d="M15 1v3" />
      <path d="M9 20v3" />
      <path d="M15 20v3" />
      <path d="M1 9h3" />
      <path d="M1 15h3" />
      <path d="M20 9h3" />
      <path d="M20 15h3" />
    </>,
  ),
  activity: P(
    <>
      <path d="M22 12h-4l-3 9L9 3l-3 9H2" />
    </>,
  ),
  battery: P(
    <>
      <rect x="2" y="7" width="16" height="10" rx="2" />
      <path d="M22 11v2" />
      <path d="M6 11v2" />
      <path d="M10 11v2" />
    </>,
  ),
  monitor: P(
    <>
      <rect x="2" y="3" width="20" height="14" rx="2" />
      <path d="M8 21h8" />
      <path d="M12 17v4" />
    </>,
  ),
  network: P(
    <>
      <circle cx="18" cy="5" r="3" />
      <circle cx="6" cy="12" r="3" />
      <circle cx="18" cy="19" r="3" />
      <path d="m8.6 10.7 6.8-4.4" />
      <path d="m8.6 13.3 6.8 4.4" />
    </>,
  ),
  hourglass: P(
    <>
      <path d="M7 2h10" />
      <path d="M7 22h10" />
      <path d="M8 2c0 5 4 5 4 10s-4 5-4 10" />
      <path d="M16 2c0 5-4 5-4 10s4 5 4 10" />
    </>,
  ),
  user: P(
    <>
      <path d="M19 21v-2a4 4 0 0 0-4-4H9a4 4 0 0 0-4 4v2" />
      <circle cx="12" cy="7" r="4" />
    </>,
  ),
  note: P(
    <>
      <path d="M15 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V7Z" />
      <path d="M14 2v4a2 2 0 0 0 2 2h4" />
      <path d="M9 13h6" />
      <path d="M9 17h6" />
    </>,
  ),
  puzzle: P(
    <>
      <path d="M19 11V8a2 2 0 0 0-2-2h-2V5a2 2 0 0 0-4 0v1H9a2 2 0 0 0-2 2v1H5a2 2 0 0 0 0 4h1v2a2 2 0 0 0 2 2h1v1a2 2 0 0 0 4 0v-1h2a2 2 0 0 0 2-2v-1h1a2 2 0 0 0 0-4Z" />
    </>,
  ),
  help: P(
    <>
      <circle cx="12" cy="12" r="10" />
      <path d="M9.09 9a3 3 0 0 1 5.83 1c0 2-3 3-3 3" />
      <path d="M12 17h.01" />
    </>,
  ),
  gauge: P(
    <>
      <path d="m12 14 4-4" />
      <path d="M3.34 19a10 10 0 1 1 17.32 0" />
    </>,
  ),
  camera: P(
    <>
      <path d="M14.5 4h-5L7 7H4a2 2 0 0 0-2 2v9a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2V9a2 2 0 0 0-2-2h-3l-2.5-3z" />
      <circle cx="12" cy="13" r="3" />
    </>,
  ),
  fileSearch: P(
    <>
      <path d="M15 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h9" />
      <path d="M14 2v4a2 2 0 0 0 2 2h4" />
      <circle cx="16.5" cy="16.5" r="3.5" />
      <path d="m19 19 2.5 2.5" />
    </>,
  ),
  smile: P(
    <>
      <circle cx="12" cy="12" r="10" />
      <path d="M8 14s1.5 2 4 2 4-2 4-2" />
      <path d="M9 9h.01" />
      <path d="M15 9h.01" />
    </>,
  ),
  asterisk: P(
    <>
      <path d="M12 6v12" />
      <path d="m3.3 9 17.4 6" />
      <path d="m3.3 15 17.4-6" />
    </>,
  ),
  palette: P(
    <>
      <path d="M12 22a10 10 0 1 1 10-10c0 1.5-1 2.5-2.5 2.5H17a2 2 0 0 0-2 2c0 .5-.5 1-1 1H9.5" />
      <path d="M7.5 10.5h.01" />
      <path d="M12 7.5h.01" />
      <path d="M16.5 10.5h.01" />
    </>,
  ),
  key: P(
    <>
      <circle cx="8" cy="15" r="4" />
      <path d="m10.8 12.2 9.2-9.2" />
      <path d="m15 5 3 3" />
    </>,
  ),
  type: P(
    <>
      <path d="M4 7V4h16v3" />
      <path d="M9 20h6" />
      <path d="M12 4v16" />
    </>,
  ),
  hash: P(
    <>
      <path d="M4 9h16" />
      <path d="M4 15h16" />
      <path d="M10 3 8 21" />
      <path d="M16 3l-2 18" />
    </>,
  ),
  text: P(
    <>
      <path d="M21 6H3" />
      <path d="M15 12H3" />
      <path d="M17 18H3" />
    </>,
  ),
  braces: P(
    <>
      <path d="M8 3H7a2 2 0 0 0-2 2v4a2 2 0 0 1-2 2 2 2 0 0 1 2 2v4a2 2 0 0 0 2 2h1" />
      <path d="M16 3h1a2 2 0 0 1 2 2v4a2 2 0 0 0 2 2 2 2 0 0 0-2 2v4a2 2 0 0 1-2 2h-1" />
    </>,
  ),
  power: P(
    <>
      <path d="M12 2v10" />
      <path d="M18.4 6.6a9 9 0 1 1-12.77.04" />
    </>,
  ),
  lock: P(
    <>
      <rect x="3" y="11" width="18" height="11" rx="2" />
      <path d="M7 11V7a5 5 0 0 1 10 0v4" />
    </>,
  ),
  moon: P(
    <>
      <path d="M12 3a6 6 0 0 0 9 9 9 9 0 1 1-9-9Z" />
    </>,
  ),
  restart: P(
    <>
      <path d="M21 12a9 9 0 1 1-9-9c2.52 0 4.93 1 6.74 2.74L21 8" />
      <path d="M21 3v5h-5" />
    </>,
  ),
  logout: P(
    <>
      <path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4" />
      <path d="m16 17 5-5-5-5" />
      <path d="M21 12H9" />
    </>,
  ),
  trash: P(
    <>
      <path d="M3 6h18" />
      <path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6" />
      <path d="M8 6V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2" />
      <path d="M10 11v6" />
      <path d="M14 11v6" />
    </>,
  ),
  enter: P(
    <>
      <path d="m9 10-5 5 5 5" />
      <path d="M20 4v7a4 4 0 0 1-4 4H4" />
    </>,
  ),
  more: P(
    <>
      <circle cx="5" cy="12" r="1" fill="currentColor" />
      <circle cx="12" cy="12" r="1" fill="currentColor" />
      <circle cx="19" cy="12" r="1" fill="currentColor" />
    </>,
  ),
  chevronRight: P(
    <>
      <path d="m9 18 6-6-6-6" />
    </>,
  ),
  link: P(
    <>
      <path d="M10 13a5 5 0 0 0 7.54.54l3-3a5 5 0 0 0-7.07-7.07l-1.72 1.71" />
      <path d="M14 11a5 5 0 0 0-7.54-.54l-3 3a5 5 0 0 0 7.07 7.07l1.71-1.71" />
    </>,
  ),
  timer: P(
    <>
      <path d="M10 2h4" />
      <path d="M12 6v4" />
      <circle cx="12" cy="14" r="8" />
      <path d="M12 14v-2" />
    </>,
  ),
  wand: P(
    <>
      <path d="m21.64 3.64-1.28 1.28a1.21 1.21 0 0 1-1.72 0L2.36 20.36a1.21 1.21 0 0 1 0-1.72l1.28-1.28" />
      <path d="m14 7 3 3" />
      <path d="M5 6v4" />
      <path d="M19 14v4" />
      <path d="M3 8h4" />
      <path d="M17 16h4" />
    </>,
  ),
  arrowRight: P(
    <>
      <path d="M5 12h14" />
      <path d="m12 5 7 7-7 7" />
    </>,
  ),
  check: P(
    <>
      <path d="M20 6 9 17l-5-5" />
    </>,
  ),
  x: P(
    <>
      <path d="M18 6 6 18" />
      <path d="m6 6 12 12" />
    </>,
  ),
  command: P(
    <>
      <path d="M15 6v12a3 3 0 1 0 3-3H6a3 3 0 1 0 3 3V6a3 3 0 1 0-3 3h12a3 3 0 1 0-3-3" />
    </>,
  ),
};

export function Icon({
  name,
  size = 20,
  strokeWidth = 2,
  className,
}: {
  name: IconName;
  size?: number;
  strokeWidth?: number;
  className?: string;
}) {
  return (
    <svg
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth={strokeWidth}
      strokeLinecap="round"
      strokeLinejoin="round"
      className={className}
      aria-hidden="true"
      focusable="false"
    >
      {PATHS[name] ?? PATHS.file}
    </svg>
  );
}

/** Map a backend result kind to a Lucide-style icon name. */
export function iconForKind(kind: string, action?: string, category?: string): IconName {
  if (action === "calc") return "calculator";
  if (action === "convert") return "convert";
  if (action === "web") return "globe";
  if (action === "habit") return "history";
  if (action === "plugin" || category === "plugin") return "puzzle";
  if (category === "clipboard" || kind === "clipboard") return "clipboardList";
  if (category === "macro" || kind === "macro") return "zap";
  if (category === "mini" && (kind === "mini" || kind === "semantic")) return "sparkles";
  switch (kind) {
    case "application":
    case "executable":
      return kind === "executable" ? "play" : "app";
    case "directory":
      return "folder";
    case "document":
      return "fileText";
    case "content":
      return "fileSearch";
    case "file":
      return "file";
    case "archive":
      return "archive";
    case "image":
      return "image";
    case "video":
      return "video";
    case "audio":
      return "music";
    case "source":
    case "config":
      return kind === "config" ? "settings" : "code";
    case "shortcut":
      return "external";
    case "browser":
    case "web":
      return "globe";
    case "bookmark":
      return "bookmark";
    case "history":
      return "history";
    case "tab":
    case "window":
      return "appWindow";
    case "calc":
      return "calculator";
    case "convert":
    case "fx":
      return "convert";
    case "clipboard":
    case "clips":
      return "clipboardList";
    case "clip":
      return "clipboardCheck";
    case "macro":
      return "zap";
    case "mini":
    case "semantic":
    case "ai":
      return "sparkles";
    case "event":
    case "calendar":
      return "clock";
    case "weather":
      return "cloudSun";
    case "time":
    case "tz":
      return "clock";
    case "disk":
    case "disku":
      return "hardDrive";
    case "ram":
      return "memoryStick";
    case "cpu":
    case "cores":
      return "cpu";
    case "process":
      return "activity";
    case "battery":
      return "battery";
    case "host":
    case "os":
    case "screen":
      return "monitor";
    case "ip":
      return "network";
    case "uptime":
      return "timer";
    case "user":
      return "user";
    case "contact":
      return "user";
    case "note":
      return "note";
    case "snippet":
      return "note";
    case "plugin":
      return "puzzle";
    case "help":
      return "help";
    case "speedtest":
      return "gauge";
    case "screenshot":
      return "camera";
    case "emoji":
      return "smile";
    case "symbol":
      return "asterisk";
    case "color":
      return "palette";
    case "uuid":
      return "key";
    case "base64":
      return "type";
    case "sha256":
      return "hash";
    case "lorem":
      return "text";
    case "json":
      return "braces";
    case "system":
    case "shutdown":
      return "power";
    case "lock":
      return "lock";
    case "sleep":
      return "moon";
    case "restart":
      return "restart";
    case "logout":
      return "logout";
    case "empty_trash":
      return "trash";
    case "swap":
      return "clipboardList";
    default:
      return "file";
  }
}
