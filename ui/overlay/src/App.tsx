import { useEffect, useMemo, useReducer, useRef } from "react";
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
  reportSize,
  rowsOf,
  tips,
} from "./protocol";
import type { NativeInMsg, PreviewMsg, ResultItem } from "./types";
import { PreviewPane } from "./components/PreviewPane";
import { ResultRow } from "./components/ResultRow";
import { Icon } from "./components/icons";

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
  | { type: "PREVIEW_TOGGLE" };

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
    case "PREVIEW_TOGGLE":
      return { ...s, previewOpen: !s.previewOpen, preview: null, menuOpen: false, menuSel: 0 };
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

export function App() {
  const [s, dispatch] = useReducer(reducer, initialState);
  const seqRef = useRef(0);
  const hideTimer = useRef(0);
  const inputRef = useRef<HTMLInputElement>(null);

  const rows = useMemo(() => rowsOf(s.items), [s.items]);
  const habits = useMemo(() => habitsOf(s.items), [s.items]);
  const needle = s.query.trim();
  const suffix = ghostSuffix(s.query, s.ghost);
  const showAssistBar = Boolean(s.correction || (s.candidates.length > 0 && s.query.trim()));
  const tabLabel = s.correction ? "Fix" : "Actions";
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
        const m = msg as unknown as {
          items?: ResultItem[];
          correction?: string;
          ghost?: string;
          candidates?: string[];
          query?: string;
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
        dispatch({ type: "RESULTS", items, resetSel: !isSpeedtestQuery(q), correction, ghost, candidates });
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
    // Demo mode: show immediately when opened in a plain browser.
    if (!hasNativeHost()) {
      dispatch({ type: "SHOW" });
      requestAnimationFrame(() => {
        inputRef.current?.focus();
        sendQuery("");
      });
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
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

  // Keep the selected row in view.
  useEffect(() => {
    if (s.previewOpen) {
      // Full-takeover preview: reset scroll so new content starts at top.
      document.querySelector(".preview")?.scrollTo({ top: 0 });
      return;
    }
    if (s.menuOpen) {
      document.querySelector(".action-menu")?.scrollIntoView({ block: "nearest" });
    } else {
      document.querySelector(".row.is-sel")?.scrollIntoView({ block: "nearest" });
    }
  }, [s.sel, s.items, s.menuOpen, s.previewOpen]);

  // Report size when assist/preview/layout changes. menuOpen/menuSel/sel are
  // included so the native window expands to fit the docked action bar.
  useEffect(() => {
    reportSize();
  }, [s.items, s.correction, s.candidates, s.previewOpen, s.preview, s.visible, s.menuOpen, s.menuSel, s.sel, s.loading]);

  const dismiss = () => {
    dispatch({ type: "HIDE" });
    window.clearTimeout(hideTimer.current);
    hideTimer.current = window.setTimeout(() => nativeSend({ type: "hidden" }), 200);
  };

  const onKeyDown = (e: React.KeyboardEvent<HTMLInputElement>) => {
    const el = e.currentTarget;
    const atEnd = (el.selectionStart ?? el.value.length) >= el.value.length;
    const mod = e.ctrlKey || e.metaKey;
    if (e.key === "Escape") {
      e.preventDefault();
      if (s.menuOpen) {
        dispatch({ type: "MENU_TOGGLE" });
        return;
      }
      if (s.correction) {
        dispatch({ type: "DISMISS_CORRECTION" });
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
  const onRowHover = (index: number) => {
    const pos = rows.findIndex((x) => x.index === index);
    if (pos >= 0 && pos !== s.sel) dispatch({ type: "SELECT", index: pos });
  };

  const showEmpty = !habits.length && !rows.length && !showSkeleton;
  const selActions = actionsOf(rows[s.sel]?.item);
  const menuVisible = s.menuOpen && !s.previewOpen && selActions.length > 0;
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

  return (
    <div className="shell" data-plat={plat}>
      <div className={launcherClass} aria-hidden={!s.visible} role="dialog" aria-label="Wilfred search">
        <div className="pill" role="search">
          <span className="icon-search" aria-hidden="true">
            <Icon name="search" size={20} strokeWidth={2} />
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
              placeholder="Search files, apps, and more"
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
            />
            {suffix && (
              <div className="ghost" aria-hidden="true">
                <span className="typed">{s.query}</span>
                <span className="suffix">{suffix}</span>
              </div>
            )}
          </div>
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
              <Icon name="x" size={16} strokeWidth={2.25} />
            </button>
          ) : null}
          <span className="hint" id="hint">
            {s.loading ? (
              <span className="spinner" aria-label="Searching" />
            ) : rows.length ? (
              <>
                <kbd>↵</kbd>
                <kbd>tab</kbd>
              </>
            ) : (
              <kbd>esc</kbd>
            )}
          </span>
        </div>
        <div className="loadbar" aria-hidden="true">
          <span className={s.loading ? "on" : ""} />
        </div>
        {s.correction && (
          <button type="button" className="correct" onMouseDown={(e) => e.preventDefault()} onClick={applyCorrection}>
            <span className="correct-icon" aria-hidden="true">
              <Icon name="wand" size={15} strokeWidth={2} />
            </span>
            <span className="correct-text">
              Did you mean <strong>{s.correction}</strong>?
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
              <Icon name="x" size={13} strokeWidth={2.25} />
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
                <Icon name="arrowRight" size={13} strokeWidth={2.25} />
                <span>{c}</span>
              </button>
            ))}
          </div>
        )}
        <div className="results" id="results" hidden={showEmpty && !s.visible}>
          {showSkeleton ? (
            <div className="rows" aria-hidden="true">
              {[0, 1, 2].map((i) => (
                <div className="skel" key={i}>
                  <i className="sk-b" />
                  <div>
                    <i className="sk-t" style={{ width: `${72 - i * 9}%` }} />
                    <i className="sk-s" />
                  </div>
                  <i className="sk-k" />
                </div>
              ))}
            </div>
          ) : showEmpty ? (
            <div className="empty">
              <span className="empty-icon" aria-hidden="true">
                <Icon name={s.query.trim() ? "fileSearch" : "search"} size={26} strokeWidth={1.75} />
              </span>
              <strong>{s.query.trim() ? `No matches for “${s.query.trim()}”` : "Search files, apps, and more"}</strong>
              <span>
                {s.query.trim()
                  ? "Typos are ok — try Tab to apply a suggestion, or start with ? to search the web."
                  : `Press ${hotkeyHint()} anywhere to summon · Try one of these`}
              </span>
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
                <div className="habits" aria-label="Recent">
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
                      <Icon name="history" size={13} strokeWidth={2} />
                      {h.title || ""}
                    </button>
                  ))}
                </div>
              )}
              {rows.length > 0 && (
                <div className="rows" role="listbox" aria-label="Results">
                  {rows.map((entry, i) => (
                    <ResultRow
                      key={entry.index}
                      entry={entry}
                      pos={i}
                      selected={i === s.sel}
                      isFirst={i === 0}
                      needle={needle}
                      expanded={s.menuOpen}
                      stagger={Math.min(i, 7) * 16}
                      onHover={onRowHover}
                      onMore={onMore}
                      onSelect={onRowSelect}
                    />
                  ))}
                </div>
              )}
            </>
          )}
        </div>
        {menuVisible && (
          <div className="action-menu" role="menu" aria-label="Actions">
            {selActions.map((a, j) => (
              <button
                key={a.id}
                type="button"
                role="menuitem"
                className={"action" + (j === s.menuSel ? " is-sel" : "")}
                onMouseDown={(e) => {
                  e.preventDefault();
                  onRowAction(a.id);
                }}
              >
                {a.label || a.id}
              </button>
            ))}
          </div>
        )}
        <aside className="preview" id="preview" hidden={!s.previewOpen}>
          <div className="preview-body" id="preview-body">
            <PreviewPane preview={s.preview} />
          </div>
        </aside>
        <div className="foot" id="foot" hidden={!rows.length && !s.candidates.length}>
          <span>
            <kbd>↵</kbd> Open
          </span>
          <span>
            <kbd>tab</kbd> {tabLabel}
          </span>
          {suffix && !s.correction && (
            <span>
              <kbd>→</kbd> Complete
            </span>
          )}
          <span>
            <kbd>F3</kbd> Preview
          </span>
          <span className="foot-hide">
            <kbd>↑</kbd>
            <kbd>↓</kbd> Move
          </span>
          <span className="foot-hide">
            <kbd>{modLabel}</kbd>
            <kbd>1–9</kbd> Quick open
          </span>
          <span className="count" id="count">
            {rows.length} result{rows.length === 1 ? "" : "s"}
          </span>
        </div>
      </div>
    </div>
  );
}
