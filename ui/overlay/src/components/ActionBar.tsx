import { Icon, Mark } from "./icons";

interface Props {
  status: string;
  primary: string;
  hasActions: boolean;
  menuOpen: boolean;
  previewOpen: boolean;
  canComplete: boolean;
  hasCorrection: boolean;
  modLabel: string;
  onPrimary: () => void;
  onActions: () => void;
  onPreview: () => void;
}

/** Bottom bar: what Enter will do, plus the secondary shortcuts. */
export function ActionBar({
  status,
  primary,
  hasActions,
  menuOpen,
  previewOpen,
  canComplete,
  hasCorrection,
  modLabel,
  onPrimary,
  onActions,
  onPreview,
}: Props) {
  return (
    <div className="bar">
      <div className="bar-status">
        <Mark size={18} />
        <span>{status}</span>
      </div>
      <div className="bar-actions">
        {hasCorrection ? (
          <span className="bar-hint">
            Fix <kbd>tab</kbd>
          </span>
        ) : canComplete ? (
          <span className="bar-hint">
            Complete <kbd>→</kbd>
          </span>
        ) : null}
        <button
          type="button"
          className={"bar-btn" + (previewOpen ? " is-on" : "")}
          tabIndex={-1}
          aria-pressed={previewOpen}
          onMouseDown={(e) => { e.preventDefault(); onPreview(); }}
        >
          <Icon name="panelRight" size={14} strokeWidth={2} />
          Details
          <kbd>F3</kbd>
        </button>
        <span className="bar-sep" aria-hidden="true" />
        <button type="button" className="bar-btn bar-primary" tabIndex={-1} onMouseDown={(e) => { e.preventDefault(); onPrimary(); }}>
          {primary}
          <kbd>↵</kbd>
        </button>
        {hasActions && (
          <button
            type="button"
            className={"bar-btn" + (menuOpen ? " is-on" : "")}
            tabIndex={-1}
            aria-haspopup="menu"
            aria-expanded={menuOpen}
            onMouseDown={(e) => { e.preventDefault(); onActions(); }}
          >
            Actions
            <kbd>{modLabel}</kbd>
            <kbd>K</kbd>
          </button>
        )}
      </div>
    </div>
  );
}
