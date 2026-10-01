import { actionsOf, groups, highlightRuns, kindOf } from "../protocol";
import { Icon, iconForKind } from "./icons";
import type { RowEntry } from "../types";

interface Props {
  entry: RowEntry;
  selected: boolean;
  isFirst: boolean;
  needle: string;
  expanded: boolean;
  onMore: (index: number) => void;
  onSelect: (index: number) => void;
}

export function ResultRow({ entry, selected, isFirst, needle, expanded, onMore, onSelect }: Props) {
  const { item, index } = entry;
  const k = kindOf(item);
  const acts = actionsOf(item);
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
  const isAnswer =
    item.action === "calc" || item.action === "convert" || (k === "speedtest" && isFirst);
  const cls = [
    "row",
    selected ? "is-sel" : "",
    item.category === "mini" ? "is-mini" : "",
    item.category === "clipboard" ? "is-clip" : "",
    isAnswer ? "is-answer" : "",
  ]
    .filter(Boolean)
    .join(" ");

  const group = groups[k] || (item.category === "mini" ? "mini" : "doc");

  return (
    <div
      className={cls}
      data-i={index}
      data-g={group}
      data-level={level}
      role="option"
      aria-selected={selected}
      onMouseEnter={() => {
        // Hover highlights without committing (selection follows hover for
        // mouse users, keyboard keeps explicit index).
      }}
      onMouseDown={(e) => {
        if ((e.target as HTMLElement).closest(".more,.action")) return;
        e.preventDefault();
        onSelect(index);
      }}
    >
      <div className="badge" aria-hidden="true">
        {item.icon ? (
          <img className="icon" alt="" src={item.icon} draggable={false} />
        ) : (
          <Icon name={iconForKind(k, item.action, item.category)} size={20} strokeWidth={2} />
        )}
      </div>
      <div className="meta">
        <div className="title">
          {highlightRuns(item.title || "", needle).map((run, i) =>
            run.mark ? <mark key={i}>{run.text}</mark> : <span key={i}>{run.text}</span>,
          )}
        </div>
        <div className="sub">{item.subtitle || item.path || ""}</div>
        {meter !== null && (
          <div className="meter" role="progressbar" aria-valuenow={meter} aria-valuemin={0} aria-valuemax={100}>
            <span style={{ width: `${meter}%` }} />
          </div>
        )}
      </div>
      <div className="kind">{k === "habit" ? "" : k}</div>
      <span className="go" aria-hidden="true">
        <Icon name="enter" size={15} strokeWidth={2.25} />
      </span>
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
          <Icon name="more" size={18} />
        </button>
      )}
    </div>
  );
}
