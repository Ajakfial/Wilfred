import { Icon, type IconName } from "./icons";
import type { ActionItem } from "../types";

interface Props {
  title: string;
  actions: ActionItem[];
  sel: number;
  onHover: (j: number) => void;
  onPick: (id: string) => void;
  onClose: () => void;
}

function iconFor(id: string): IconName {
  if (id === "open" || id.startsWith("open_with")) return "external";
  if (id === "reveal") return "folderOpen";
  if (id === "copy_path" || id === "copy_text" || id.startsWith("copy")) return "copy";
  if (id === "paste") return "clipboardCheck";
  if (id === "preview") return "eye";
  if (id === "delete" || id === "trash") return "trash";
  return "zap";
}

/** Floating action popover anchored above the action bar (Raycast ⌘K). */
export function ActionMenu({ title, actions, sel, onHover, onPick, onClose }: Props) {
  return (
    <>
      <div className="scrim" onMouseDown={(e) => { e.preventDefault(); onClose(); }} aria-hidden="true" />
      <div className="action-menu" role="menu" aria-label={`Actions for ${title}`}>
        <div className="am-title" title={title}>{title}</div>
        {actions.map((a, j) => (
          <button
            key={a.id}
            type="button"
            role="menuitem"
            className={"action" + (j === sel ? " is-sel" : "")}
            tabIndex={-1}
            onMouseMove={() => onHover(j)}
            onMouseDown={(e) => {
              e.preventDefault();
              onPick(a.id);
            }}
          >
            <span className="action-ico" aria-hidden="true">
              <Icon name={iconFor(a.id)} size={15} strokeWidth={2} />
            </span>
            <span className="action-label">{a.label || a.id}</span>
            {j === 0 ? <kbd>↵</kbd> : j === 1 ? <><kbd>⇧</kbd><kbd>↵</kbd></> : null}
          </button>
        ))}
      </div>
    </>
  );
}
