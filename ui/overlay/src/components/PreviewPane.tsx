import { iconForKind, Icon } from "./icons";
import type { PreviewMsg } from "../types";

interface Props {
  preview: PreviewMsg | null;
  /** Full path of the selected item (the preview message doesn't carry it). */
  path?: string;
}

/** Detail pane shown beside the list (F3). Mirrors Raycast's list-detail:
 *  title, content, then a hairline-separated metadata table. */
export function PreviewPane({ preview, path }: Props) {
  const title = preview?.title || "";
  const kind = preview?.kind || "";
  const kindIcon = iconForKind((kind || "file").toLowerCase());
  const meta: Array<[string, string]> = [];
  if (kind) meta.push(["Kind", kind.charAt(0).toUpperCase() + kind.slice(1)]);
  if (preview?.size) meta.push(["Size", preview.size]);
  if (preview?.modified) meta.push(["Modified", preview.modified]);

  return (
    <div className="pv">
      <div className="pv-head">
        <span className="pv-badge" aria-hidden="true">
          <Icon name={kindIcon} size={15} strokeWidth={2.1} />
        </span>
        <div className="pv-title" title={title}>
          {title || "Preview"}
        </div>
      </div>
      <div className="pv-content">
        {preview?.image ? (
          <img className="pv-img" alt="" src={preview.image} draggable={false} />
        ) : preview?.text ? (
          <pre className="pv-text">{preview.text}</pre>
        ) : (
          <div className="pv-empty">
            <Icon name="eye" size={20} strokeWidth={1.75} />
            <span>{preview?.error || "Select a file to see its contents"}</span>
          </div>
        )}
      </div>
      {(meta.length > 0 || path) && (
        <dl className="pv-meta">
          {path ? (
            <div className="pv-row">
              <dt>Where</dt>
              <dd className="pv-path" title={path}>
                {path}
              </dd>
            </div>
          ) : null}
          {meta.map(([k, v]) => (
            <div className="pv-row" key={k}>
              <dt>{k}</dt>
              <dd>{v}</dd>
            </div>
          ))}
        </dl>
      )}
    </div>
  );
}
