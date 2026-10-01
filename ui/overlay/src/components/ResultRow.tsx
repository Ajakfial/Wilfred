import { actionsOf, badges, groups, highlightRuns, kindOf } from "../protocol";
import type { RowEntry } from "../types";

interface Props {
  entry: RowEntry;
  selected: boolean;
  isFirst: boolean;
  needle: string;
  menuOpen: boolean;
  menuSel: number;
  onMore: (index: number) => void;
  onAction: (actionId: string) => void;
  onSelect: (index: number) => void;
}

export function ResultRow({ entry, selected, isFirst, needle, menuOpen, menuSel, onMore, onAction, onSelect }: Props) {
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

  return (
    <div
      className={cls}
      data-i={index}
      data-g={groups[k] || (item.category === "mini" ? "mini" : "doc")}
      data-level={level}
      onMouseDown={(e) => {
        if ((e.target as HTMLElement).closest(".more,.action")) return;
        e.preventDefault();
        onSelect(index);
      }}
    >
      <div className="badge">
        {item.icon ? (
          <img className="icon" alt="" src={item.icon} />
        ) : (
          <i className={"bi " + (badges[k] || badges.file)} aria-hidden="true" />
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
          <div className="meter">
            <span style={{ width: `${meter}%` }} />
          </div>
        )}
      </div>
      <div className="kind">{k === "habit" ? "" : k}</div>
      {acts.length > 0 && (
        <>
          <button
            type="button"
            className="more"
            aria-label="Actions"
            onMouseDown={(e) => {
              e.preventDefault();
              onMore(index);
            }}
          >
            <i className="bi bi-three-dots" />
          </button>
          <div className="action-menu" hidden={!(menuOpen && selected)}>
            {acts.map((a, j) => (
              <button
                key={a.id}
                type="button"
                className={"action" + (j === menuSel && selected && menuOpen ? " is-sel" : "")}
                onMouseDown={(e) => {
                  e.preventDefault();
                  onAction(a.id);
                }}
              >
                {a.label || a.id}
              </button>
            ))}
          </div>
        </>
      )}
    </div>
  );
}
