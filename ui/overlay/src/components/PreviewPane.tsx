import { iconForKind, Icon } from "./icons";
import type { PreviewMsg } from "../types";

export function PreviewPane({ preview }: { preview: PreviewMsg | null }) {
  const title = preview?.title || "";
  const kind = preview?.kind || "";
  const meta = [preview?.size, preview?.modified].filter(Boolean).join("  ·  ");
  const kindIcon = iconForKind((kind || "file").toLowerCase());
  return (
    <div className="pv-card">
      <div className="pv-head">
        <span className="pv-badge" aria-hidden="true">
          <Icon name={kindIcon} size={17} strokeWidth={2} />
        </span>
        <div className="pv-title" title={title}>{title || "Preview"}</div>
      </div>
      <div className="pv-meta">
        {kind}
        {meta ? `  ·  ${meta}` : ""}
        {!kind && !meta ? "Select a file to preview" : ""}
      </div>
      {preview?.image ? (
        <img className="pv-img" alt="" src={preview.image} draggable={false} />
      ) : preview?.text ? (
        <pre className="pv-text">{preview.text}</pre>
      ) : (
        <div className="pv-empty">{preview?.error || "Select a file to preview · F3 toggles"}</div>
      )}
    </div>
  );
}
