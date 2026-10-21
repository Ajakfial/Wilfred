import { Icon, Mark } from "./icons";
import { chordLabel, keybindFor, uiStr } from "../protocol";

interface Props {
  status: string;
  primary: string;
  hasActions: boolean;
  menuOpen: boolean;
  previewOpen: boolean;
  canComplete: boolean;
  hasCorrection: boolean;
  modLabel: string;
  settingsOpen: boolean;
  /** Size of the current multi-selection (0 = single-select mode). */
  selectionCount: number;
  onClearSelection: () => void;
  onPrimary: () => void;
  onActions: () => void;
  onPreview: () => void;
  onSettings: () => void;
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
  settingsOpen,
  selectionCount,
  onClearSelection,
  onPrimary,
  onActions,
  onPreview,
  onSettings,
}: Props) {
  const previewKey = chordLabel(keybindFor("preview"), modLabel);
  const actionsKey = chordLabel(keybindFor("actions"), modLabel);
  return (
    <div className="bar">
      <div className="bar-status">
        <Mark size={18} />
        {selectionCount > 0 ? (
          <button
            type="button"
            className="bar-chip"
            tabIndex={-1}
            title={uiStr("overlay.selection_clear", "Clear selection")}
            onMouseDown={(e) => {
              e.preventDefault();
              onClearSelection();
            }}
          >
            {uiStr("overlay.selection_count", "{n} selected", { n: selectionCount })}
            <Icon name="x" size={11} strokeWidth={2.6} />
          </button>
        ) : (
          <span>{status}</span>
        )}
      </div>
      <div className="bar-actions">
        {hasCorrection ? (
          <span className="bar-hint">
            {uiStr("overlay.bar_fix", "Fix")} <kbd>{chordLabel(keybindFor("complete"), modLabel)}</kbd>
          </span>
        ) : canComplete ? (
          <span className="bar-hint">
            {uiStr("overlay.bar_complete", "Complete")} <kbd>→</kbd>
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
          {uiStr("overlay.bar_details", "Details")}
          <kbd>{previewKey}</kbd>
        </button>
        <span className="bar-sep" aria-hidden="true" />
        <button
          type="button"
          className={"bar-btn" + (settingsOpen ? " is-on" : "")}
          tabIndex={-1}
          aria-pressed={settingsOpen}
          aria-label={uiStr("overlay.settings_title", "Settings")}
          onMouseDown={(e) => {
            e.preventDefault();
            onSettings();
          }}
        >
          <Icon name="settings" size={14} strokeWidth={2} />
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
            {uiStr("overlay.bar_actions", "Actions")}
            <kbd>{actionsKey}</kbd>
          </button>
        )}
      </div>
    </div>
  );
}
