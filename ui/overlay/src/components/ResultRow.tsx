import type { ReactNode } from "react";
import { actionsOf, groupOf, highlightRuns, kindLabel, kindOf, parseAnswer, parseSpeed } from "../protocol";
import { Icon, iconForKind } from "./icons";
import type { RowEntry } from "../types";

interface Props {
  entry: RowEntry;
  /** Position in the visible list (a11y + quick-open number). */
  pos: number;
  selected: boolean;
  needle: string;
  expanded: boolean;
  /** Label for the quick-open chord ("⌘" / "Ctrl") while the modifier is held. */
  quickMod: string | null;
  onHover: (index: number, x: number, y: number) => void;
  onMore: (index: number) => void;
  onSelect: (index: number) => void;
}

function Marked({ text, needle }: { text: string; needle: string }) {
  return (
    <>
      {highlightRuns(text, needle).map((run, i) =>
        run.mark ? <mark key={i}>{run.text}</mark> : <span key={i}>{run.text}</span>,
      )}
    </>
  );
}

/** Size bucket so long results (UUIDs, hashes) never overflow the card. */
function answerSize(text: string): "xl" | "l" | "m" | "s" {
  const n = text.length;
  if (n <= 12) return "xl";
  if (n <= 22) return "l";
  if (n <= 40) return "m";
  return "s";
}

export function ResultRow({ entry, pos, selected, needle, expanded, quickMod, onHover, onMore, onSelect }: Props) {
  const { item, index } = entry;
  const k = kindOf(item);
  const acts = actionsOf(item);
  const group = groupOf(item);
  const answer = parseAnswer(item);
  const speed = parseSpeed(item);
  const meter =
    typeof item.meter === "number" && Number.isFinite(item.meter) && item.meter >= 0
      ? Math.max(0, Math.min(100, item.meter))
      : null;
  const level =
    ["disk", "disku", "ram", "cpu", "swap"].includes(k) && meter !== null
      ? meter >= 85
        ? "high"
        : meter >= 65
          ? "mid"
          : "low"
      : undefined;
  const variant = answer ? "answer" : speed ? "speed" : "item";
  const cls = [
    "row",
    `row--${variant}`,
    selected ? "is-sel" : "",
    meter !== null && variant === "item" ? "has-meter" : "",
    item.category === "clipboard" ? "is-clip" : "",
  ]
    .filter(Boolean)
    .join(" ");

  const sub = item.subtitle || item.path || "";
  const kindText = kindLabel(k);
  const quick = quickMod && pos < 9 ? `${quickMod}${quickMod.length > 1 ? " " : ""}${pos + 1}` : "";

  let body: ReactNode;
  if (answer) {
    body = (
      <>
        <div className="ans-in">
          <span className="ans-label">{answer.label}</span>
          <span className="ans-expr" title={answer.expression}>
            {answer.expression || "—"}
          </span>
        </div>
        <span className="ans-arrow" aria-hidden="true">
          <Icon name="arrowRight" size={14} strokeWidth={2.25} />
        </span>
        <div className="ans-out" data-size={answerSize(answer.result)} title={answer.result}>
          {answer.result}
        </div>
      </>
    );
  } else if (speed) {
    const stat = (label: string, icon: "arrowDown" | "arrowUp" | null, v: { value: string; unit: string }) => (
      <div className="sp-stat">
        <span className="sp-label">
          {icon && <Icon name={icon} size={12} strokeWidth={2.5} />}
          {label}
        </span>
        <span className="sp-value">
          <b className={v.value === "—" ? "is-idle" : ""}>{v.value}</b>
          {v.unit && <i>{v.unit}</i>}
        </span>
      </div>
    );
    body = (
      <>
        <div className="sp-stats">
          {stat("Download", "arrowDown", speed.down)}
          {stat("Upload", "arrowUp", speed.up)}
          {stat("Ping", null, speed.ping)}
        </div>
        <div className="sp-foot">
          <div
            className={"sp-track" + (speed.done ? " is-done" : "")}
            role="progressbar"
            aria-valuenow={speed.progress}
            aria-valuemin={0}
            aria-valuemax={100}
          >
            <span style={{ width: `${speed.progress}%` }} />
          </div>
          <span className="sp-phase">{speed.phase}</span>
        </div>
      </>
    );
  } else {
    body = (
      <>
        <div className={"tile" + (item.icon ? " has-img" : "")} aria-hidden="true">
          {item.icon ? (
            <img className="icon" alt="" src={item.icon} draggable={false} />
          ) : (
            <Icon name={iconForKind(k, item.action, item.category)} size={16} strokeWidth={2.1} />
          )}
        </div>
        <div className="meta">
          <div className="line">
            <span className="title">
              <Marked text={item.title || ""} needle={needle} />
            </span>
            {sub ? (
              <span className="sub" title={sub}>
                {sub}
              </span>
            ) : null}
          </div>
          {meter !== null && (
            <div className="meter" role="progressbar" aria-valuenow={meter} aria-valuemin={0} aria-valuemax={100}>
              <span style={{ width: `${meter}%` }} />
            </div>
          )}
        </div>
        {quick ? (
          <kbd className="quick">{quick}</kbd>
        ) : kindText ? (
          <span className="kind">{kindText}</span>
        ) : null}
        {acts.length > 0 && (
          <button
            type="button"
            className="more"
            aria-label="Actions"
            aria-expanded={expanded && selected}
            tabIndex={-1}
            onMouseDown={(e) => {
              e.preventDefault();
              onMore(index);
            }}
          >
            <Icon name="more" size={16} />
          </button>
        )}
      </>
    );
  }

  return (
    <div
      id={`row-${index}`}
      className={cls}
      data-i={index}
      data-pos={pos}
      data-g={group}
      data-level={level}
      role="option"
      aria-selected={selected}
      onMouseMove={(e) => onHover(index, e.clientX, e.clientY)}
      onMouseDown={(e) => {
        if ((e.target as HTMLElement).closest(".more")) return;
        e.preventDefault();
        onSelect(index);
      }}
    >
      {body}
      {variant !== "item" && quick ? <kbd className="quick quick--card">{quick}</kbd> : null}
    </div>
  );
}
