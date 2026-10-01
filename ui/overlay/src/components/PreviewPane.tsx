import type { PreviewMsg } from "../types";

export function PreviewPane({ preview }: { preview: PreviewMsg | null }) {
  const title = preview?.title || "";
  const kind = preview?.kind || "";
  const meta = [preview?.size, preview?.modified].filter(Boolean).join("  ·  ");
  return (
    <>
      <div className="pv-title">{title}</div>
      <div className="pv-meta">
        {kind}
        {meta ? `  ·  ${meta}` : ""}
      </div>
      {preview?.image ? (
        <img className="pv-img" alt="" src={preview.image} />
      ) : preview?.text ? (
        <pre className="pv-text">{preview.text}</pre>
      ) : (
        <div className="pv-empty">{preview?.error || "No preview available"}</div>
      )}
    </>
  );
}
