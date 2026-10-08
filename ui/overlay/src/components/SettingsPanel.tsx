import { useMemo, useState } from "react";
import { uiStr } from "../protocol";
import { Icon } from "./icons";

export interface SettingSchema {
  key: string;
  type: "bool" | "int" | "double" | "string" | "list";
  secret: boolean;
  live: boolean;
  options: string[];
}

export interface SettingResult {
  ok: boolean;
  error?: string;
  value?: string;
}

interface Props {
  schema: SettingSchema[];
  values: Record<string, string>;
  results: Record<string, SettingResult>;
  pending: Record<string, boolean>;
  onSet: (key: string, value: string) => void;
  onClose: () => void;
}

function shortLabel(key: string): string {
  const dot = key.indexOf(".");
  return dot >= 0 ? key.slice(dot + 1).replace(/_/g, " ") : key;
}

function Row({
  meta,
  value,
  result,
  busy,
  onSet,
}: {
  meta: SettingSchema;
  value: string;
  result?: SettingResult;
  busy: boolean;
  onSet: (key: string, value: string) => void;
}) {
  const [draft, setDraft] = useState<string | null>(null);
  const shown = draft ?? value ?? "";
  const dirty = draft !== null && draft !== (value ?? "");
  const commit = (v: string) => {
    setDraft(null);
    if (v !== (value ?? "")) onSet(meta.key, v);
  };

  let editor: React.ReactNode;
  if (meta.type === "bool") {
    const on = (value ?? "") === "true";
    editor = (
      <button
        type="button"
        role="switch"
        aria-checked={on}
        aria-label={meta.key}
        className={"tgl" + (on ? " is-on" : "")}
        disabled={busy}
        onMouseDown={(e) => {
          e.preventDefault();
          onSet(meta.key, on ? "false" : "true");
        }}
      >
        <span className="tgl-knob" />
      </button>
    );
  } else if (meta.options.length > 0) {
    editor = (
      <select
        aria-label={meta.key}
        value={shown}
        disabled={busy}
        onMouseDown={(e) => e.stopPropagation()}
        onChange={(e) => commit(e.target.value)}
      >
        {meta.options.map((o) => (
          <option key={o} value={o}>
            {o}
          </option>
        ))}
      </select>
    );
  } else if (meta.type === "int" || meta.type === "double") {
    editor = (
      <input
        type="number"
        aria-label={meta.key}
        value={shown}
        disabled={busy}
        onChange={(e) => setDraft(e.target.value)}
        onBlur={(e) => {
          if (dirty) commit(e.target.value);
          else setDraft(null);
        }}
        onKeyDown={(e) => {
          if (e.key === "Enter") commit((e.target as HTMLInputElement).value);
          if (e.key === "Escape") setDraft(null);
          e.stopPropagation();
        }}
        onMouseDown={(e) => e.stopPropagation()}
      />
    );
  } else {
    editor = (
      <input
        type={meta.secret ? "password" : "text"}
        aria-label={meta.key}
        value={meta.secret && draft === null ? "" : shown}
        placeholder={meta.secret ? "unchanged" : undefined}
        disabled={busy}
        onChange={(e) => setDraft(e.target.value)}
        onBlur={(e) => {
          if (dirty) commit(e.target.value);
          else setDraft(null);
        }}
        onKeyDown={(e) => {
          if (e.key === "Enter") commit((e.target as HTMLInputElement).value);
          if (e.key === "Escape") setDraft(null);
          e.stopPropagation();
        }}
        onMouseDown={(e) => e.stopPropagation()}
      />
    );
  }

  return (
    <div className={"set-row" + (dirty ? " is-dirty" : "")}>
      <div className="set-meta">
        <span className="set-key" title={meta.key}>
          {shortLabel(meta.key)}
        </span>
        <span className="set-flags">
          {meta.live ? (
            <span className="chip mini-chip">{uiStr("overlay.settings_live", "Applies instantly")}</span>
          ) : null}
          {result && !result.ok ? <span className="set-err">{result.error || "error"}</span> : null}
          {result?.ok && !dirty ? (
            <span className="chip mini-chip ok">{uiStr("overlay.settings_saved", "Saved")}</span>
          ) : null}
          {busy ? <span className="spinner tiny" /> : null}
        </span>
      </div>
      <div className="set-ctl">{editor}</div>
    </div>
  );
}

export function SettingsPanel({ schema, values, results, pending, onSet, onClose }: Props) {
  const [filter, setFilter] = useState("");
  const groups = useMemo(() => {
    const needle = filter.trim().toLowerCase();
    const map = new Map<string, SettingSchema[]>();
    for (const m of schema) {
      if (needle && !m.key.toLowerCase().includes(needle)) continue;
      const dot = m.key.indexOf(".");
      const sec = dot >= 0 ? m.key.slice(0, dot) : "misc";
      const list = map.get(sec);
      if (list) list.push(m);
      else map.set(sec, [m]);
    }
    return [...map.entries()];
  }, [schema, filter]);

  return (
    <div className="settings">
      <div className="set-head">
        <span className="set-title">
          <Icon name="settings" size={15} strokeWidth={2.1} />
          {uiStr("overlay.settings_title", "Settings")}
        </span>
        <input
          className="set-filter"
          value={filter}
          onChange={(e) => setFilter(e.target.value)}
          placeholder={uiStr("overlay.settings_search", "Filter settings")}
          aria-label={uiStr("overlay.settings_search", "Filter settings")}
          onKeyDown={(e) => {
            if (e.key === "Escape") {
              if (filter) setFilter("");
              else onClose();
            }
            e.stopPropagation();
          }}
          onMouseDown={(e) => e.stopPropagation()}
        />
        <button type="button" className="bar-btn" onMouseDown={(e) => e.preventDefault()} onClick={onClose}>
          {uiStr("overlay.settings_back", "Back to search")}
        </button>
      </div>
      <p className="muted set-note">
        {uiStr(
          "overlay.settings_note",
          "Keys marked “{live}” apply on next summon; the rest {restart}.",
          {
            live: uiStr("overlay.settings_live", "Applies instantly"),
            restart: uiStr("overlay.settings_restart", "Needs daemon restart").toLowerCase(),
          },
        )}
      </p>
      <div className="set-body">
        {groups.length === 0 ? (
          <p className="muted">No settings match.</p>
        ) : (
          groups.map(([sec, rows]) => (
            <div className="sec" key={sec}>
              <div className="sec-h">{sec}</div>
              {rows.map((m) => (
                <Row
                  key={m.key}
                  meta={m}
                  value={values[m.key] ?? ""}
                  result={results[m.key]}
                  busy={!!pending[m.key]}
                  onSet={onSet}
                />
              ))}
            </div>
          ))
        )}
      </div>
    </div>
  );
}
