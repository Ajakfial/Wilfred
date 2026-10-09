import { useEffect, useLayoutEffect, useMemo, useReducer, useRef, useState } from "react";
import { demoAssist, demoQuery } from "./demo";
import {
  actionsOf,
  ghostSuffix,
  habitsOf,
  hasNativeHost,
  hotkeyHint,
  isSpeedtestQuery,
  nativeSend,
  platformId,
  primaryLabel,
  reportSize,
  enterFitMode,
  rowsOf,
  sectionsOf,
  setUiStrings,
  tips,
  uiStr,
} from "./protocol";
import type { NativeInMsg, PreviewMsg, ResultItem } from "./types";
import { ActionBar } from "./components/ActionBar";
import { ActionMenu } from "./components/ActionMenu";
import { PreviewPane } from "./components/PreviewPane";
import { ResultRow } from "./components/ResultRow";
import { SettingsPanel } from "./components/SettingsPanel";
import type { SettingResult, SettingSchema } from "./components/SettingsPanel";
import { Icon } from "./components/icons";

interface SettingsMsg {
  schema?: SettingSchema[];
  values?: Record<string, string>;
}

interface SettingResultMsg {
  key?: string;
  ok?: boolean;
  error?: string;
  value?: string;
}

interface State {
  items: ResultItem[];
  sel: number;
  visible: boolean;
  entered: boolean;
  menuOpen: boolean;
  menuSel: number;
  previewOpen: boolean;
  preview: PreviewMsg | null;
  query: string;
  correction: string;
  ghost: string;
  candidates: string[];
  loading: boolean;
  settingsOpen: boolean;
  settingsSchema: SettingSchema[];
  settingsValues: Record<string, string>;
  settingResults: Record<string, SettingResult>;
  settingPending: Record<string, boolean>;
}

const initialState: State = {
  items: [],
  sel: 0,
  visible: false,
  entered: false,
  menuOpen: false,
  menuSel: 0,
  previewOpen: false,
  preview: null,
  query: "",
  correction: "",
  ghost: "",
  candidates: [],
  loading: false,
  settingsOpen: false,
  settingsSchema: [],
  settingsValues: {},
  settingResults: {},
  settingPending: {},
};

type Action =
  | { type: "SHOW" }
  | { type: "HIDE" }
  | { type: "RESULTS"; items: ResultItem[]; resetSel: boolean; correction: string; ghost: string; candidates: string[] }
  | { type: "PREVIEW"; preview: PreviewMsg | null }
  | { type: "QUERY"; query: string }
  | { type: "APPLY_ASSIST"; query: string }
  | { type: "DISMISS_CORRECTION" }
  | { type: "NAV"; delta: number; count: number }
  | { type: "SELECT"; index: number }
  | { type: "MENU_TOGGLE" }
  | { type: "MENU_CYCLE"; delta: number; count: number }
  | { type: "MENU_SET"; index: number }
  | { type: "PREVIEW_TOGGLE" }
  | { type: "SETTINGS_OPEN" }
  | { type: "SETTINGS_CLOSE" }
  | { type: "SETTINGS_DATA"; schema: SettingSchema[]; values: Record<string, string> }
  | { type: "SETTING_SEND"; key: string }
  | { type: "SETTING_RESULT"; key: string; ok: boolean; error: string; value?: string };

function reducer(s: State, a: Action): State {
  switch (a.type) {
    case "SHOW":
      return { ...initialState, visible: true, entered: true, previewOpen: s.previewOpen, preview: null };
    case "HIDE":
      return { ...s, visible: false, loading: false };
    case "RESULTS": {
      const count = rowsOf(a.items).length;
      const sel = a.resetSel ? 0 : Math.max(0, Math.min(s.sel, Math.max(0, count - 1)));
      return {
        ...s,
        items: a.items,
        sel,
        menuOpen: false,
        menuSel: 0,
        correction: a.correction,
        ghost: a.ghost,
        candidates: a.candidates,
        loading: false,
      };
    }
    case "PREVIEW":
      return { ...s, preview: a.preview };
    case "QUERY":
      return { ...s, query: a.query, loading: true };
    case "APPLY_ASSIST":
      return { ...s, query: a.query, correction: "", loading: true };
    case "DISMISS_CORRECTION":
      return { ...s, correction: "" };
    case "NAV": {
      if (a.count === 0) return s;
      const sel = (s.sel + a.delta + a.count) % a.count;
      return { ...s, sel, menuOpen: false, menuSel: 0 };
    }
    case "SELECT":
      if (a.index === s.sel) return s;
      return { ...s, sel: a.index, menuOpen: false, menuSel: 0 };
    case "MENU_TOGGLE":
      return { ...s, menuOpen: !s.menuOpen, menuSel: 0 };
    case "MENU_CYCLE": {
      if (a.count === 0) return s;
      return { ...s, menuSel: (s.menuSel + a.delta + a.count) % a.count };
    }
    case "MENU_SET":
      return a.index === s.menuSel ? s : { ...s, menuSel: a.index };
    case "PREVIEW_TOGGLE":
      return { ...s, previewOpen: !s.previewOpen, preview: null, menuOpen: false, menuSel: 0 };
    case "SETTINGS_OPEN":
      return { ...s, settingsOpen: true, menuOpen: false };
    case "SETTINGS_CLOSE":
      return { ...s, settingsOpen: false };
    case "SETTINGS_DATA":
      return { ...s, settingsSchema: a.schema, settingsValues: a.values };
    case "SETTING_SEND": {
      const pending = { ...s.settingPending, [a.key]: true };
      const results = { ...s.settingResults };
      delete results[a.key];
      return { ...s, settingPending: pending, settingResults: results };
    }
    case "SETTING_RESULT": {
      const pending = { ...s.settingPending };
      delete pending[a.key];
      const results = { ...s.settingResults, [a.key]: { ok: a.ok, error: a.error } };
      const values = { ...s.settingsValues };
      if (a.ok && a.value !== undefined) values[a.key] = a.value;
      return { ...s, settingPending: pending, settingResults: results, settingsValues: values };
    }
  }
}

function previewableIndex(items: ResultItem[], sel: number): number {
  const rows = rowsOf(items);
  if (!rows.length || sel < 0 || sel >= rows.length) return -1;
  const item = rows[sel].item;
  if (!item || item.action === "habit") return -1;
  const p = item.path || item.payload || "";
  if (!p || p.startsWith("http://") || p.startsWith("https://")) return -1;
  return rows[sel].index;
}

// Background appearance driven by `ui:` config in wilfred.yml, delivered
// via native `show` / `config` messages (see overlay_show_json). Defaults
// preserve the glass look; transparent:false adds html.opaque for a solid
// panel, blur:false adds html.no-blur, opacity scales the glass alpha.
let gAppearanceOpacity = 1;
let gAppearanceTransparent = true;

function clampAppearanceOpacity(v: unknown): number | null {
  if (typeof v === "number" && Number.isFinite(v)) return Math.min(1, Math.max(0, v));
  if (typeof v === "string") {
    const n = parseFloat(v);
    if (Number.isFinite(n)) return Math.min(1, Math.max(0, n));
  }
  return null;
}

function applyGlassOpacity(opacity: number, transparent: boolean): void {
  const root = document.documentElement;
  if (!transparent || opacity >= 1) {
    root.style.removeProperty("--glass");
    root.style.removeProperty("--pop-glass");
    return;
  }
  const light =
    typeof window.matchMedia === "function" &&
    window.matchMedia("(prefers-color-scheme: light)").matches;
  // Base alphas mirror style.css defaults.
  const glassA = (light ? 0.8 : 0.74) * opacity;
  const popA = (light ? 0.96 : 0.95) * opacity;
  const glassRgb = light ? "252, 252, 254" : "21, 22, 28";
  const popRgb = light ? "255, 255, 255" : "34, 35, 44";
  root.style.setProperty("--glass", `rgba(${glassRgb}, ${glassA.toFixed(3)})`);
  root.style.setProperty("--pop-glass", `rgba(${popRgb}, ${popA.toFixed(3)})`);
}

function hexToRgb(v: string): [number, number, number] | null {
  let h = v.trim().toLowerCase();
  if (h.startsWith("#")) h = h.slice(1);
  if (/^[0-9a-f]{3}$/.test(h)) {
    const r = parseInt(h[0] + h[0], 16);
    const g = parseInt(h[1] + h[1], 16);
    const b = parseInt(h[2] + h[2], 16);
    return [r, g, b];
  }
  if (/^[0-9a-f]{6}$/.test(h)) {
    return [parseInt(h.slice(0, 2), 16), parseInt(h.slice(2, 4), 16), parseInt(h.slice(4, 6), 16)];
  }
  return null;
}

const kNamedAccents: Record<string, [number, number, number]> = {
  indigo: [127, 140, 255],
  blue: [80, 140, 250],
  green: [40, 200, 150],
  teal: [40, 190, 220],
  pink: [240, 100, 160],
  orange: [245, 140, 70],
  red: [240, 100, 100],
  purple: [150, 120, 250],
  violet: [171, 144, 255],
};

function applyAccent(accent: unknown): void {
  const root = document.documentElement;
  if (typeof accent !== "string" || !accent.trim()) {
    root.style.removeProperty("--accent");
    root.style.removeProperty("--accent-rgb");
    root.style.removeProperty("--accent-ink");
    return;
  }
  const raw = accent.trim();
  const rgb = hexToRgb(raw) || kNamedAccents[raw.toLowerCase()];
  if (!rgb) return;
  const hex = "#" + rgb.map((v) => v.toString(16).padStart(2, "0")).join("");
  root.style.setProperty("--accent", hex);
  root.style.setProperty("--accent-rgb", `${rgb[0]} ${rgb[1]} ${rgb[2]}`);
  root.style.setProperty("--accent-ink", hex);
}

function applyFontSize(size: unknown): void {
  const root = document.documentElement;
  const n = typeof size === "number" ? size : typeof size === "string" ? parseFloat(size) : NaN;
  if (!Number.isFinite(n) || n <= 0) {
    root.style.removeProperty("--base-font");
    root.style.removeProperty("font-size");
    document.body.style.removeProperty("font-size");
    return;
  }
  const px = Math.min(24, Math.max(10, Math.round(n)));
  root.style.setProperty("--base-font", `${px}px`);
  document.body.style.fontSize = `${px}px`;
}

function applyAppearance(m: { transparent?: unknown; opacity?: unknown; blur?: unknown; accent?: unknown; fontSize?: unknown }): void {
  const root = document.documentElement;
  if (typeof m.transparent === "boolean") {
    gAppearanceTransparent = m.transparent;
    root.classList.toggle("opaque", !m.transparent);
  }
  const transparent = !root.classList.contains("opaque");
  const op = clampAppearanceOpacity(m.opacity);
  if (op !== null) gAppearanceOpacity = op;
  if (typeof m.blur === "boolean") {
    // blur:false (or opaque panel) disables backdrop blur.
    root.classList.toggle("no-blur", !m.blur || !transparent);
  } else if (typeof m.transparent === "boolean") {
    // Transparent toggled without an explicit blur flag: opaque implies no-blur.
    if (!transparent) root.classList.add("no-blur");
  }
  applyGlassOpacity(gAppearanceOpacity, transparent);
  if (!transparent) {
    root.style.removeProperty("--glass");
    root.style.removeProperty("--pop-glass");
  }
  if ("accent" in m) applyAccent(m.accent);
  if ("fontSize" in m) applyFontSize(m.fontSize);
}

export function App() {
  const [s, dispatch] = useReducer(reducer, initialState);
  const seqRef = useRef(0);
  const hideTimer = useRef(0);
  const inputRef = useRef<HTMLInputElement>(null);
  const rowsRef = useRef<HTMLDivElement>(null);
  const pillRef = useRef<HTMLDivElement>(null);
  const prevRowsRef = useRef<unknown>(null);
  const mouseRef = useRef({ x: -1, y: -1 });
  const [modHeld, setModHeld] = useState(false);

  const rows = useMemo(() => rowsOf(s.items), [s.items]);
  const sections = useMemo(() => sectionsOf(rows), [rows]);
  const showHeads = sections.length > 1;
  const habits = useMemo(() => habitsOf(s.items), [s.items]);
  const needle = s.query.trim();
  const suffix = ghostSuffix(s.query, s.ghost);
  const showAssistBar = Boolean(s.correction || (s.candidates.length > 0 && s.query.trim()));
  const showSkeleton = s.loading && rows.length === 0 && Boolean(s.query.trim());

  const sendQuery = (q: string) => {
    const id = ++seqRef.current;
    nativeSend({ type: "query", q, id });
    if (!hasNativeHost()) {
      const items = demoQuery(q);
      const assist = demoAssist(q);
      window.setTimeout(() => {
        if (id === seqRef.current)
          dispatch({
            type: "RESULTS",
            items,
            resetSel: !isSpeedtestQuery(q),
            correction: assist.correction,
            ghost: assist.ghost,
            candidates: assist.candidates,
          });
      }, 40);
    }
  };

  const applyText = (text: string) => {
    dispatch({ type: "APPLY_ASSIST", query: text });
    inputRef.current?.focus();
    // Move caret to end after React commits.
    requestAnimationFrame(() => {
      const el = inputRef.current;
      if (el) {
        el.setSelectionRange(el.value.length, el.value.length);
      }
    });
    sendQuery(text);
  };

  const applyHabit = (text: string) => {
    applyText(text.endsWith(" ") ? text : text + " ");
  };

  const submit = (actionId?: string, rowPos?: number) => {
    if (!rows.length) return;
    const pos = rowPos ?? s.sel;
    const item = rows[pos]?.item;
    if (!item) return;
    if (item.action === "habit") {
      applyHabit(((item.payload || item.title || "") as string).trimEnd() + " ");
      return;
    }
    dispatch({ type: "SELECT", index: pos });
    nativeSend({ type: "submit", index: rows[pos].index, action: actionId || "" });
  };

  const toggleMenu = (rowPos?: number) => {
    const pos = rowPos ?? s.sel;
    const item = rows[pos]?.item;
    if (!item || !actionsOf(item).length) return;
    if (rowPos !== undefined && rowPos !== s.sel) dispatch({ type: "SELECT", index: rowPos });
    dispatch({ type: "MENU_TOGGLE" });
  };

  const acceptGhost = () => {
    if (!s.ghost || s.ghost.length <= s.query.length) return;
    // Only accept when the ghost truly extends the query (prefix case).
    if (s.ghost.toLowerCase().startsWith(s.query.toLowerCase())) applyText(s.ghost);
    else if (s.candidates.length) applyText(s.candidates[0]);
  };

  const applyCorrection = () => {
    if (s.correction) applyText(s.correction + (s.correction.endsWith(" ") ? "" : " "));
  };

  // Native host messages (registered once; dispatch is stable).
  useEffect(() => {
    const onNative = (msg: NativeInMsg) => {
      if (!msg || typeof msg !== "object") return;
      if (msg.type === "show") {
        setUiStrings((msg as unknown as { strings?: unknown }).strings);
        applyAppearance(msg as { transparent?: unknown; opacity?: unknown; blur?: unknown });
        window.clearTimeout(hideTimer.current);
        dispatch({ type: "SHOW" });
        requestAnimationFrame(() => {
          inputRef.current?.focus();
          reportSize();
          sendQuery("");
        });
      } else if (msg.type === "config") {
        setUiStrings((msg as unknown as { strings?: unknown }).strings);
        applyAppearance(msg as { transparent?: unknown; opacity?: unknown; blur?: unknown });
      } else if (msg.type === "fit") {
        enterFitMode();
      } else if (msg.type === "hide") {
        setModHeld(false);
        dispatch({ type: "HIDE" });
      } else if (msg.type === "preview") {
        dispatch({ type: "PREVIEW", preview: msg as PreviewMsg });
      } else if (msg.type === "settings") {
        const m = msg as unknown as SettingsMsg;
        dispatch({
          type: "SETTINGS_DATA",
          schema: Array.isArray(m.schema) ? m.schema : [],
          values: m.values && typeof m.values === "object" ? m.values : {},
        });
      } else if (msg.type === "setting-result") {
        const m = msg as unknown as SettingResultMsg;
        if (typeof m.key === "string") {
          dispatch({
            type: "SETTING_RESULT",
            key: m.key,
            ok: m.ok === true,
            error: typeof m.error === "string" ? m.error : "",
            value: typeof m.value === "string" ? m.value : undefined,
          });
        }
      } else if (msg.type === "results") {
        const m = msg as unknown as {
          items?: ResultItem[];
          correction?: string;
          ghost?: string;
          candidates?: string[];
          query?: string;
          update?: boolean;
        };
        const items = Array.isArray(m.items) ? m.items : [];
        const correction = typeof m.correction === "string" ? m.correction : "";
        const ghost = typeof m.ghost === "string" ? m.ghost : "";
        const candidates = Array.isArray(m.candidates)
          ? m.candidates.filter((c): c is string => typeof c === "string").slice(0, 6)
          : [];
        // Stale guard: if the host echoes the query and the input moved on,
        // still show results (they're the latest the host has) but don't
        // clobber an in-flight newer query's assist when visibly stale.
        const q = inputRef.current?.value ?? "";
        const stale = typeof m.query === "string" && m.query !== q && !isSpeedtestQuery(q);
        void stale;
        // Late provider merge for the displayed query: replace the list but
        // keep the keyboard selection where it is.
        const update = m.update === true;
        dispatch({ type: "RESULTS", items, resetSel: !update && !isSpeedtestQuery(q), correction, ghost, candidates });
      }
    };
    (window as unknown as { __wilfredNative?: (m: NativeInMsg) => void }).__wilfredNative = onNative;
    const wv = (window as unknown as {
      chrome?: { webview?: { addEventListener: (t: string, f: (e: { data: unknown }) => void) => void } };
    }).chrome?.webview;
    wv?.addEventListener("message", (e) => {
      let d = e.data;
      if (typeof d === "string") {
        try {
          d = JSON.parse(d);
        } catch {
          return;
        }
      }
      onNative(d as NativeInMsg);
    });
    nativeSend({ type: "ready" });
    // Re-derive glass colors if the OS color scheme changes after a custom
    // opacity was applied (applyGlassOpacity picks dark/light base colors).
    let schemeMq: MediaQueryList | null = null;
    const onScheme = () => applyGlassOpacity(gAppearanceOpacity, gAppearanceTransparent);
    if (typeof window.matchMedia === "function") {
      schemeMq = window.matchMedia("(prefers-color-scheme: light)");
      if (typeof schemeMq.addEventListener === "function") schemeMq.addEventListener("change", onScheme);
      else if (typeof (schemeMq as unknown as { addListener: (f: () => void) => void }).addListener === "function")
        (schemeMq as unknown as { addListener: (f: () => void) => void }).addListener(onScheme);
    }
    // Demo mode: show immediately when opened in a plain browser.
    if (!hasNativeHost()) {
      dispatch({ type: "SHOW" });
      requestAnimationFrame(() => {
        inputRef.current?.focus();
        sendQuery("");
      });
    }
    return () => {
      if (schemeMq) {
        if (typeof schemeMq.removeEventListener === "function")
          schemeMq.removeEventListener("change", onScheme);
        else if (
          typeof (schemeMq as unknown as { removeListener: (f: () => void) => void }).removeListener ===
          "function"
        )
          (schemeMq as unknown as { removeListener: (f: () => void) => void }).removeListener(onScheme);
      }
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Keep the window sized to content until the user drags an edge; after that
  // the panel fills whatever size they chose (see protocol.ts / html.fit).
  useEffect(() => {
    const shell = document.querySelector(".shell");
    if (!shell || typeof ResizeObserver === "undefined") {
      reportSize();
      return;
    }
    const ro = new ResizeObserver(() => reportSize());
    ro.observe(shell);
    return () => ro.disconnect();
  }, []);

  // Live speedtest refresh.
  useEffect(() => {
    if (!s.visible || !isSpeedtestQuery(s.query)) return;
    const t = window.setInterval(() => {
      const id = ++seqRef.current;
      if (!hasNativeHost()) {
        const items = demoQuery(s.query);
        const assist = demoAssist(s.query);
        window.setTimeout(() => {
          if (id === seqRef.current)
            dispatch({
              type: "RESULTS",
              items,
              resetSel: false,
              correction: assist.correction,
              ghost: assist.ghost,
              candidates: assist.candidates,
            });
        }, 40);
      } else {
        nativeSend({ type: "query", q: s.query, id });
      }
    }, 350);
    return () => window.clearInterval(t);
  }, [s.visible, s.query]);

  // Debounced preview refresh on selection / results change.
  useEffect(() => {
    if (!s.previewOpen || !s.visible) return;
    const t = window.setTimeout(() => {
      const idx = previewableIndex(s.items, s.sel);
      if (idx < 0) {
        dispatch({ type: "PREVIEW", preview: { type: "preview", error: "Nothing to preview" } });
      } else {
        nativeSend({ type: "preview", index: idx });
        if (!hasNativeHost()) {
          const rowsNow = rowsOf(s.items);
          const it = rowsNow[s.sel]?.item;
          const p = it?.path || it?.payload || "";
          dispatch({
            type: "PREVIEW",
            preview: {
              type: "preview",
              title: it?.title || p,
              kind: it?.kind || "file",
              size: "12.4 KB",
              modified: "2026-09-30 10:42",
              text: p ? `${p}\n\nDemo preview — open via the native host for full content.` : "",
            },
          });
        }
      }
    }, 150);
    return () => window.clearTimeout(t);
  }, [s.sel, s.previewOpen, s.items, s.visible]);

  // Sliding selection pill: one element glides to the selected row instead of
  // each row repainting its own background. Snaps (no glide) when the list
  // itself changed so typing never makes the highlight swim across new rows.
  useLayoutEffect(() => {
    const host = rowsRef.current;
    const pill = pillRef.current;
    if (!host || !pill) return;
    const el = host.querySelector<HTMLElement>(".row.is-sel");
    if (!el) {
      pill.style.opacity = "0";
      pill.dataset.on = "0";
      return;
    }
    const changed = prevRowsRef.current !== rows;
    prevRowsRef.current = rows;
    const snap = changed || pill.dataset.on !== "1";
    if (snap) pill.classList.add("snap");
    const h = host.getBoundingClientRect();
    const r = el.getBoundingClientRect();
    pill.dataset.card = el.classList.contains("row--item") ? "0" : "1";
    pill.style.height = `${r.height}px`;
    pill.style.transform = `translateY(${r.top - h.top + host.scrollTop}px)`;
    pill.style.opacity = "1";
    pill.dataset.on = "1";
    if (snap) {
      requestAnimationFrame(() => requestAnimationFrame(() => pill.classList.remove("snap")));
    }
  }, [s.sel, rows, s.previewOpen, s.visible, showHeads, s.query]);

  // Keep the selected row in view.
  useEffect(() => {
    if (s.previewOpen) {
      document.querySelector(".detail")?.scrollTo({ top: 0 });
    }
    document.querySelector(".row.is-sel")?.scrollIntoView({ block: "nearest" });
  }, [s.sel, s.items, s.previewOpen]);

  // Releasing focus (alt-tab, click-away) must not leave the ⌘/Ctrl hints stuck on.
  useEffect(() => {
    const off = () => setModHeld(false);
    window.addEventListener("blur", off);
    return () => window.removeEventListener("blur", off);
  }, []);

  // Report size when assist/preview/layout changes. menuOpen/menuSel/sel are
  // included so the native window expands to fit the docked action bar.
  useEffect(() => {
    reportSize();
  }, [s.items, s.correction, s.candidates, s.previewOpen, s.preview, s.visible, s.menuOpen, s.menuSel, s.sel, s.loading, s.settingsOpen, s.settingsSchema]);

  const dismiss = () => {
    setModHeld(false);
    dispatch({ type: "HIDE" });
    window.clearTimeout(hideTimer.current);
    hideTimer.current = window.setTimeout(() => nativeSend({ type: "hidden" }), 200);
  };

  const toggleSettings = () => {
    if (s.settingsOpen) {
      dispatch({ type: "SETTINGS_CLOSE" });
      return;
    }
    dispatch({ type: "SETTINGS_OPEN" });
    nativeSend({ type: "settings-get" });
    if (!hasNativeHost()) {
      window.setTimeout(() => {
        dispatch({ type: "SETTINGS_DATA", schema: [], values: {} });
      }, 40);
    }
  };

  const sendSetting = (key: string, value: string) => {
    dispatch({ type: "SETTING_SEND", key });
    nativeSend({ type: "setting-set", key, value });
    if (!hasNativeHost()) {
      window.setTimeout(() => {
        dispatch({ type: "SETTING_RESULT", key, ok: true, error: "", value });
      }, 40);
    }
  };

  const onKeyDown = (e: React.KeyboardEvent<HTMLInputElement>) => {
    const el = e.currentTarget;
    const atEnd = (el.selectionStart ?? el.value.length) >= el.value.length;
    const mod = e.ctrlKey || e.metaKey;
    if (e.key === "Control" || e.key === "Meta") setModHeld(true);
    if (e.key === "Escape") {
      e.preventDefault();
      if (s.settingsOpen) {
        dispatch({ type: "SETTINGS_CLOSE" });
        return;
      }
      if (s.menuOpen) {
        dispatch({ type: "MENU_TOGGLE" });
        return;
      }
      if (s.correction) {
        dispatch({ type: "DISMISS_CORRECTION" });
        return;
      }
      if (s.previewOpen) {
        dispatch({ type: "PREVIEW_TOGGLE" });
        return;
      }
      dismiss();
    } else if (e.key === "Tab") {
      e.preventDefault();
      // Typo-first: a confident correction wins over the action menu.
      if (s.correction) {
        applyCorrection();
        return;
      }
      if (suffix) {
        acceptGhost();
        return;
      }
      toggleMenu();
    } else if (e.key === "ArrowRight" && !s.menuOpen) {
      if (suffix && atEnd) {
        e.preventDefault();
        acceptGhost();
        return;
      }
    } else if (e.key === "ArrowRight" && s.menuOpen) {
      e.preventDefault();
      const n = actionsOf(rows[s.sel]?.item).length;
      if (n) dispatch({ type: "MENU_CYCLE", delta: 1, count: n });
    } else if (e.key === "ArrowLeft" && s.menuOpen) {
      e.preventDefault();
      const n = actionsOf(rows[s.sel]?.item).length;
      if (n) dispatch({ type: "MENU_CYCLE", delta: -1, count: n });
    } else if ((e.key === "ArrowDown" || e.key === "ArrowUp") && s.menuOpen && !mod) {
      // The actions popover is a vertical list: arrows move inside it.
      e.preventDefault();
      const n = actionsOf(rows[s.sel]?.item).length;
      if (n) dispatch({ type: "MENU_CYCLE", delta: e.key === "ArrowDown" ? 1 : -1, count: n });
    } else if (e.key === "ArrowDown") {
      e.preventDefault();
      if (e.ctrlKey || e.metaKey) {
        if (rows.length) dispatch({ type: "SELECT", index: rows.length - 1 });
      } else if (rows.length) dispatch({ type: "NAV", delta: 1, count: rows.length });
    } else if (e.key === "ArrowUp") {
      e.preventDefault();
      if (e.ctrlKey || e.metaKey) {
        if (rows.length) dispatch({ type: "SELECT", index: 0 });
      } else if (rows.length) dispatch({ type: "NAV", delta: -1, count: rows.length });
    } else if (e.key === "Home") {
      if (rows.length) {
        e.preventDefault();
        dispatch({ type: "SELECT", index: 0 });
      }
    } else if (e.key === "End") {
      if (rows.length) {
        e.preventDefault();
        dispatch({ type: "SELECT", index: rows.length - 1 });
      }
    } else if (e.key === "PageDown") {
      if (rows.length) {
        e.preventDefault();
        dispatch({ type: "NAV", delta: Math.min(8, rows.length - 1), count: rows.length });
      }
    } else if (e.key === "PageUp") {
      if (rows.length) {
        e.preventDefault();
        dispatch({ type: "NAV", delta: -Math.min(8, rows.length - 1), count: rows.length });
      }
    } else if (e.key === "F3") {
      e.preventDefault();
      dispatch({ type: "PREVIEW_TOGGLE" });
    } else if (e.key === "Enter") {
      e.preventDefault();
      const item = rows[s.sel]?.item;
      if (s.menuOpen) {
        const act = actionsOf(item)[s.menuSel];
        submit(act ? act.id : "");
      } else if (e.shiftKey || e.altKey) {
        const acts = actionsOf(item);
        submit(acts[1] ? acts[1].id : acts[0] ? acts[0].id : "");
      } else {
        submit();
      }
    } else if (mod && (e.key === "n" || e.key === "N")) {
      e.preventDefault();
      if (rows.length) dispatch({ type: "NAV", delta: 1, count: rows.length });
    } else if (mod && (e.key === "p" || e.key === "P")) {
      e.preventDefault();
      if (rows.length) dispatch({ type: "NAV", delta: -1, count: rows.length });
    } else if (mod && (e.key === "k" || e.key === "K")) {
      // Raycast convention: ⌘K / Ctrl+K opens the actions popover.
      e.preventDefault();
      toggleMenu();
    } else if (e.ctrlKey && (e.key === "u" || e.key === "U")) {
      // Clear the query (was ⌘/Ctrl+K before the actions popover took it).
      e.preventDefault();
      applyText("");
    } else if (mod && e.key >= "1" && e.key <= "9") {
      // Raycast-style quick open: Ctrl/Cmd+1..9 opens that row.
      const pos = Number(e.key) - 1;
      if (pos < rows.length) {
        e.preventDefault();
        submit(undefined, pos);
      }
    }
  };

  const onKeyUp = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === "Control" || e.key === "Meta") setModHeld(false);
  };

  const onTip = (t: string) => applyHabit(t);
  const onHabit = (title: string) => applyHabit(title);
  const onMore = (index: number) => {
    const pos = rows.findIndex((x) => x.index === index);
    if (pos >= 0) toggleMenu(pos);
  };
  const onRowAction = (actionId: string) => submit(actionId);
  const onRowSelect = (index: number) => {
    const pos = rows.findIndex((x) => x.index === index);
    if (pos >= 0) submit(undefined, pos);
  };
  // Hover selects, but only on genuine pointer movement: a list that scrolls
  // under a resting cursor must not steal the keyboard selection.
  const onRowHover = (index: number, x: number, y: number) => {
    if (mouseRef.current.x === x && mouseRef.current.y === y) return;
    mouseRef.current = { x, y };
    const pos = rows.findIndex((x2) => x2.index === index);
    if (pos >= 0 && pos !== s.sel) dispatch({ type: "SELECT", index: pos });
  };

  const showEmpty = !habits.length && !rows.length && !showSkeleton;
  const selItem = rows[s.sel]?.item;
  const selActions = actionsOf(selItem);
  const menuVisible = s.menuOpen && selActions.length > 0;
  const launcherClass = [
    "launcher",
    s.visible ? "is-in" : s.entered ? "is-out" : "",
    s.previewOpen ? "has-preview" : "",
    menuVisible ? "has-menu" : "",
  ]
    .filter(Boolean)
    .join(" ");
  const plat = platformId();
  const modLabel = plat === "mac" ? "⌘" : "Ctrl";
  const activeId = rows[s.sel] ? `row-${rows[s.sel].index}` : undefined;
  const status =
    rows.length === 1
      ? uiStr("overlay.bar_results_one", "{n} result", { n: rows.length })
      : uiStr("overlay.bar_results_other", "{n} results", { n: rows.length });
  const selPath = selItem && selItem.action !== "habit" ? selItem.path || "" : "";

  return (
    <div className="shell" data-plat={plat}>
      <div
        className={launcherClass}
        style={{ ["--menu-n" as string]: selActions.length }}
        aria-hidden={!s.visible}
        role="dialog"
        aria-label="Wilfred search"
      >
        <div className="search" role="search">
          <span className="search-ico" aria-hidden="true">
            <Icon name="search" size={19} strokeWidth={2.1} />
          </span>
          <div className="field">
            <input
              ref={inputRef}
              id="q"
              type="text"
              autoComplete="off"
              autoCorrect="off"
              autoCapitalize="off"
              spellCheck={false}
              placeholder={uiStr("overlay.search_placeholder", "Search files, apps, and more")}
              aria-label="Search"
              aria-autocomplete="both"
              aria-expanded={showAssistBar}
              aria-controls="results"
              aria-activedescendant={activeId}
              value={s.query}
              onChange={(e) => {
                const q = e.target.value;
                dispatch({ type: "QUERY", query: q });
                sendQuery(q);
              }}
              onKeyDown={onKeyDown}
              onKeyUp={onKeyUp}
            />
            {suffix && (
              <div className="ghost" aria-hidden="true">
                <span className="typed">{s.query}</span>
                <span className="suffix">{suffix}</span>
              </div>
            )}
          </div>
          {s.loading ? <span className="spinner" role="status" aria-label="Searching" /> : null}
          {s.query ? (
            <button
              type="button"
              className="clear"
              aria-label="Clear search"
              tabIndex={-1}
              onMouseDown={(e) => {
                e.preventDefault();
                applyText("");
              }}
            >
              <Icon name="x" size={14} strokeWidth={2.4} />
            </button>
          ) : (
            <kbd className="esc-hint">esc</kbd>
          )}
        </div>
        <div className="loadbar" aria-hidden="true">
          <span className={s.loading ? "on" : ""} />
        </div>
        {s.correction && (
          <button type="button" className="correct" onMouseDown={(e) => e.preventDefault()} onClick={applyCorrection}>
            <span className="correct-icon" aria-hidden="true">
              <Icon name="wand" size={13} strokeWidth={2.2} />
            </span>
            <span className="correct-text">
              {uiStr("overlay.correct_prefix", "Did you mean")} <strong>{s.correction}</strong>?
            </span>
            <kbd>tab</kbd>
            <span
              className="correct-x"
              role="button"
              aria-label="Dismiss correction"
              onMouseDown={(e) => e.preventDefault()}
              onClick={(e) => {
                e.stopPropagation();
                dispatch({ type: "DISMISS_CORRECTION" });
                inputRef.current?.focus();
              }}
            >
              <Icon name="x" size={12} strokeWidth={2.4} />
            </span>
          </button>
        )}
        {showAssistBar && !s.correction && s.candidates.length > 0 && (
          <div className="cands" role="listbox" aria-label="Autocomplete">
            {s.candidates.slice(0, 6).map((c) => (
              <button
                key={c}
                type="button"
                role="option"
                aria-selected={false}
                className="cand"
                tabIndex={-1}
                onMouseDown={(e) => {
                  e.preventDefault();
                  applyText(c);
                }}
              >
                <Icon name="arrowRight" size={12} strokeWidth={2.4} />
                <span>{c}</span>
              </button>
            ))}
          </div>
        )}
        <div className="body">
          {s.settingsOpen ? (
            <div className="results" id="results">
              <SettingsPanel
                schema={s.settingsSchema}
                values={s.settingsValues}
                results={s.settingResults}
                pending={s.settingPending}
                onSet={sendSetting}
                onClose={() => dispatch({ type: "SETTINGS_CLOSE" })}
              />
            </div>
          ) : (
            <>
              <div className="results" id="results" hidden={showEmpty && !s.visible}>
            {showSkeleton ? (
              <div className="rows" aria-hidden="true">
                {[0, 1, 2, 3].map((i) => (
                  <div className="skel" key={i}>
                    <i className="sk-b" />
                    <i className="sk-t" style={{ width: `${64 - i * 9}%` }} />
                    <i className="sk-k" />
                  </div>
                ))}
              </div>
            ) : showEmpty ? (
              <div className="empty">
                <span className="empty-icon" aria-hidden="true">
                  <Icon name={s.query.trim() ? "fileSearch" : "search"} size={22} strokeWidth={1.8} />
                </span>
                <strong>
                  {s.query.trim()
                    ? uiStr("overlay.empty_title_none", "No results for “{q}”", { q: s.query.trim() })
                    : uiStr("overlay.empty_title_idle", "Search, calculate, or run a command")}
                </strong>
                <span>
                  {s.query.trim()
                    ? s.correction
                      ? uiStr(
                          "overlay.empty_sub_none_correction",
                          "Press Tab to use the suggestion above, or start with ? to search the web.",
                        )
                      : uiStr(
                          "overlay.empty_sub_none",
                          "Check the spelling, or start with ? to search the web.",
                        )
                    : uiStr("overlay.empty_sub_idle", "Press {hotkey} anywhere to open Wilfred. Try one of these:", {
                        hotkey: hotkeyHint(),
                      })}
                </span>
                {!s.query.trim() && (
                  <div className="chips chips--center">
                    {tips.map((t) => (
                      <button key={t} type="button" className="chip" onMouseDown={(e) => { e.preventDefault(); onTip(t); }}>
                        {t}
                      </button>
                    ))}
                  </div>
                )}
              </div>
            ) : (
              <>
                {habits.length > 0 && (
                  <div className="chips" aria-label="Recent searches">
                    {habits.map((h, i) => (
                      <button
                        key={i}
                        type="button"
                        className="chip"
                        onMouseDown={(e) => {
                          e.preventDefault();
                          onHabit(h.title || "");
                        }}
                      >
                        <Icon name="history" size={12} strokeWidth={2.2} />
                        {h.title || ""}
                      </button>
                    ))}
                  </div>
                )}
                {rows.length > 0 && (
                  <div className="rows" role="listbox" aria-label="Results" ref={rowsRef}>
                    <div className="pill" ref={pillRef} aria-hidden="true" />
                    {sections.map((sec) => (
                      <div className="sec" role="group" aria-label={sec.label} key={sec.key}>
                        {showHeads && <div className="sec-h">{sec.label}</div>}
                        {sec.rows.map(({ entry, pos }) => (
                          <ResultRow
                            key={entry.index}
                            entry={entry}
                            pos={pos}
                            selected={pos === s.sel}
                            needle={needle}
                            expanded={s.menuOpen}
                            quickMod={modHeld ? modLabel : null}
                            onHover={onRowHover}
                            onMore={onMore}
                            onSelect={onRowSelect}
                          />
                        ))}
                      </div>
                    ))}
                  </div>
                )}
              </>
            )}
          </div>
            </>
          )}
          <aside className="detail" id="preview" hidden={!s.previewOpen} aria-label="Details">
            <PreviewPane preview={s.preview} path={selPath} />
          </aside>
        </div>
        {(rows.length > 0 || s.settingsOpen) && (
          <ActionBar
            status={s.settingsOpen ? uiStr("overlay.settings_title", "Settings") : status}
            primary={primaryLabel(selItem)}
            hasActions={selActions.length > 0}
            menuOpen={menuVisible}
            previewOpen={s.previewOpen}
            canComplete={Boolean(suffix)}
            hasCorrection={Boolean(s.correction)}
            modLabel={modLabel}
            settingsOpen={s.settingsOpen}
            onPrimary={() => submit()}
            onActions={() => toggleMenu()}
            onPreview={() => dispatch({ type: "PREVIEW_TOGGLE" })}
            onSettings={toggleSettings}
          />
        )}
        {menuVisible && (
          <ActionMenu
            title={selItem?.title || "Actions"}
            actions={selActions}
            sel={s.menuSel}
            onHover={(j) => dispatch({ type: "MENU_SET", index: j })}
            onPick={onRowAction}
            onClose={() => dispatch({ type: "MENU_TOGGLE" })}
          />
        )}
      </div>
    </div>
  );
}
