import { useEffect, useMemo, useReducer, useRef } from "react";
import { demoQuery } from "./demo";
import { actionsOf, habitsOf, hasNativeHost, isSpeedtestQuery, nativeSend, reportSize, rowsOf, tips } from "./protocol";
import type { NativeInMsg, PreviewMsg, ResultItem } from "./types";
import { PreviewPane } from "./components/PreviewPane";
import { ResultRow } from "./components/ResultRow";

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
};

type Action =
  | { type: "SHOW" }
  | { type: "HIDE" }
  | { type: "RESULTS"; items: ResultItem[]; resetSel: boolean }
  | { type: "PREVIEW"; preview: PreviewMsg | null }
  | { type: "QUERY"; query: string }
  | { type: "NAV"; delta: number; count: number }
  | { type: "SELECT"; index: number }
  | { type: "MENU_TOGGLE" }
  | { type: "MENU_CYCLE"; delta: number; count: number }
  | { type: "PREVIEW_TOGGLE" };

function reducer(s: State, a: Action): State {
  switch (a.type) {
    case "SHOW":
      return { ...initialState, visible: true, entered: true, previewOpen: s.previewOpen, preview: null };
    case "HIDE":
      return { ...s, visible: false };
    case "RESULTS": {
      const count = rowsOf(a.items).length;
      const sel = a.resetSel ? 0 : Math.max(0, Math.min(s.sel, Math.max(0, count - 1)));
      return { ...s, items: a.items, sel, menuOpen: false, menuSel: 0 };
    }
    case "PREVIEW":
      return { ...s, preview: a.preview };
    case "QUERY":
      return { ...s, query: a.query };
    case "NAV": {
      if (a.count === 0) return s;
      const sel = (s.sel + a.delta + a.count) % a.count;
      return { ...s, sel, menuOpen: false, menuSel: 0 };
    }
    case "SELECT":
      return { ...s, sel: a.index, menuOpen: false, menuSel: 0 };
    case "MENU_TOGGLE":
      return { ...s, menuOpen: !s.menuOpen, menuSel: 0 };
    case "MENU_CYCLE": {
      if (a.count === 0) return s;
      return { ...s, menuSel: (s.menuSel + a.delta + a.count) % a.count };
    }
    case "PREVIEW_TOGGLE":
      return { ...s, previewOpen: !s.previewOpen, preview: null };
  }
}

function previewableIndex(
  items: ResultItem[],
  sel: number,
): number {
  const rows = rowsOf(items);
  if (!rows.length || sel < 0 || sel >= rows.length) return -1;
  const item = rows[sel].item;
  if (!item || item.action === "habit") return -1;
  const p = item.path || item.payload || "";
  if (!p || p.startsWith("http://") || p.startsWith("https://")) return -1;
  return rows[sel].index;
}

export function App() {
  const [s, dispatch] = useReducer(reducer, initialState);
  const seqRef = useRef(0);
  const hideTimer = useRef(0);
  const inputRef = useRef<HTMLInputElement>(null);

  const rows = useMemo(() => rowsOf(s.items), [s.items]);
  const habits = useMemo(() => habitsOf(s.items), [s.items]);
  const needle = s.query.trim();

  const sendQuery = (q: string) => {
    const id = ++seqRef.current;
    nativeSend({ type: "query", q, id });
    if (!hasNativeHost()) {
      const items = demoQuery(q);
      window.setTimeout(() => {
        if (id === seqRef.current) dispatch({ type: "RESULTS", items, resetSel: !isSpeedtestQuery(q) });
      }, 40);
    }
  };

  const applyHabit = (text: string) => {
    dispatch({ type: "QUERY", query: text });
    inputRef.current?.focus();
    sendQuery(text);
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

  // Native host messages (registered once; dispatch is stable).
  useEffect(() => {
    const onNative = (msg: NativeInMsg) => {
      if (!msg || typeof msg !== "object") return;
      if (msg.type === "show") {
        window.clearTimeout(hideTimer.current);
        dispatch({ type: "SHOW" });
        requestAnimationFrame(() => {
          inputRef.current?.focus();
          reportSize();
          sendQuery("");
        });
      } else if (msg.type === "hide") {
        dispatch({ type: "HIDE" });
      } else if (msg.type === "preview") {
        dispatch({ type: "PREVIEW", preview: msg as PreviewMsg });
      } else if (msg.type === "results") {
        const items = Array.isArray((msg as { items?: ResultItem[] }).items)
          ? (msg as { items: ResultItem[] }).items
          : [];
        // Read the live input value: speedtest streams must not reset selection.
        const q = inputRef.current?.value ?? "";
        dispatch({ type: "RESULTS", items, resetSel: !isSpeedtestQuery(q) });
      }
    };
    // macOS/Linux hosts call this directly via script evaluation.
    (window as unknown as { __wilfredNative?: (m: NativeInMsg) => void }).__wilfredNative = onNative;
    // Windows (WebView2) delivers PostWebMessageAsJson through this event.
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
  }, []);

  // Keep the window sized to content.
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
        window.setTimeout(() => {
          if (id === seqRef.current) dispatch({ type: "RESULTS", items, resetSel: false });
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
      }
    }, 150);
    return () => window.clearTimeout(t);
  }, [s.sel, s.previewOpen, s.items, s.visible]);

  // Keep the selected row in view.
  useEffect(() => {
    document.querySelector(".row.is-sel")?.scrollIntoView({ block: "nearest" });
  }, [s.sel, s.items]);

  const dismiss = () => {
    dispatch({ type: "HIDE" });
    window.clearTimeout(hideTimer.current);
    hideTimer.current = window.setTimeout(() => nativeSend({ type: "hidden" }), 200);
  };

  const onKeyDown = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === "Escape") {
      e.preventDefault();
      if (s.menuOpen) {
        dispatch({ type: "MENU_TOGGLE" });
        return;
      }
      dismiss();
    } else if (e.key === "Tab") {
      e.preventDefault();
      toggleMenu();
    } else if (e.key === "ArrowRight" && s.menuOpen) {
      e.preventDefault();
      const n = actionsOf(rows[s.sel]?.item).length;
      if (n) dispatch({ type: "MENU_CYCLE", delta: 1, count: n });
    } else if (e.key === "ArrowLeft" && s.menuOpen) {
      e.preventDefault();
      const n = actionsOf(rows[s.sel]?.item).length;
      if (n) dispatch({ type: "MENU_CYCLE", delta: -1, count: n });
    } else if (e.key === "ArrowDown") {
      e.preventDefault();
      if (rows.length) dispatch({ type: "NAV", delta: 1, count: rows.length });
    } else if (e.key === "ArrowUp") {
      e.preventDefault();
      if (rows.length) dispatch({ type: "NAV", delta: -1, count: rows.length });
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
    }
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

  const showEmpty = !habits.length && !rows.length;
  const launcherClass = ["launcher", s.visible ? "is-in" : s.entered ? "is-out" : "", s.previewOpen ? "has-preview" : ""]
    .filter(Boolean)
    .join(" ");

  return (
    <div className="shell">
      <div className={launcherClass} aria-hidden={!s.visible}>
        <div className="pill" role="search">
          <span className="icon-search" aria-hidden="true">
            <i className="bi bi-search" />
          </span>
          <input
            ref={inputRef}
            id="q"
            type="text"
            autoComplete="off"
            spellCheck={false}
            placeholder="Search files, apps, and more"
            aria-label="Search"
            value={s.query}
            onChange={(e) => {
              const q = e.target.value;
              dispatch({ type: "QUERY", query: q });
              sendQuery(q);
            }}
            onKeyDown={onKeyDown}
          />
          <kbd className="hint" id="hint">
            {rows.length ? "↵  tab" : "esc"}
          </kbd>
        </div>
        <div className="results" id="results" hidden={showEmpty && !s.visible}>
          {showEmpty ? (
            <div className="empty">
              <strong>{s.query.trim() ? `No matches for “${s.query.trim()}”` : "Search files, apps, and more"}</strong>
              <span>{s.query.trim() ? "Start with ? to search the web instead." : "Try one of these"}</span>
              {!s.query.trim() && (
                <div className="habits">
                  {tips.map((t) => (
                    <button key={t} type="button" className="tip" onMouseDown={(e) => { e.preventDefault(); onTip(t); }}>
                      {t}
                    </button>
                  ))}
                </div>
              )}
            </div>
          ) : (
            <>
              {habits.length > 0 && (
                <div className="habits">
                  {habits.map((h, i) => (
                    <button
                      key={i}
                      type="button"
                      className="habit"
                      onMouseDown={(e) => {
                        e.preventDefault();
                        onHabit(h.title || "");
                      }}
                    >
                      {h.title || ""}
                    </button>
                  ))}
                </div>
              )}
              {rows.length > 0 && (
                <div className="rows">
                  {rows.map((entry, i) => (
                    <ResultRow
                      key={entry.index}
                      entry={entry}
                      selected={i === s.sel}
                      isFirst={i === 0}
                      needle={needle}
                      menuOpen={s.menuOpen}
                      menuSel={s.menuSel}
                      onMore={onMore}
                      onAction={onRowAction}
                      onSelect={onRowSelect}
                    />
                  ))}
                </div>
              )}
            </>
          )}
        </div>
        <aside className="preview" id="preview" hidden={!s.previewOpen}>
          <div className="preview-body" id="preview-body">
            <PreviewPane preview={s.preview} />
          </div>
        </aside>
        <div className="foot" id="foot" hidden={!rows.length}>
          <span>
            <kbd>↵</kbd> Open
          </span>
          <span>
            <kbd>tab</kbd> Actions
          </span>
          <span>
            <kbd>F3</kbd> Preview
          </span>
          <span>
            <kbd>↑</kbd>
            <kbd>↓</kbd> Move
          </span>
          <span>
            <kbd>esc</kbd> Close
          </span>
          <span className="count" id="count">
            {rows.length} result{rows.length === 1 ? "" : "s"}
          </span>
        </div>
      </div>
    </div>
  );
}
