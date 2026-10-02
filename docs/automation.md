# System-Wide Automation

Wilfred automation has three layers: things you trigger from anywhere
without opening the overlay, things scripts trigger without the overlay,
and multi-step workflows that tie actions together.

## Extra global hotkeys (`hotkeys:`)

Besides the summon hotkey (`hotkey:`), `hotkeys:` maps binding names to
`{modifiers, key, run}` triples, each registered with the OS and fired
from any app:

```yaml
hotkeys:
  google-clip:
    modifiers: [ctrl, alt]
    key: G
    run: macro:gclip
  lock-now:
    modifiers: [ctrl, alt]
    key: L
    run: system:lock
```

`run` is one of:

| Form | Effect |
|---|---|
| `show` | Toggle the overlay (same as the summon hotkey) |
| `macro:<text>` | Expand a macro/quicklink with the clipboard as argument, then open the URL |
| `system:<id>` | Run a session command (`lock`, `sleep`, `shutdown`, `restart`, `logout`, `empty_trash`) |
| `media:<id>` | Press a media key (`play`, `pause`, `playpause`, `next`, `prev`, `stop`, `mute`, `volup`, `voldn`) |
| `workflow:<name>` | Run a named workflow against the clipboard |

Workflows bound to hotkeys run headless against the clipboard, so prefer
targetless steps there — file steps (e.g. `copy_path`) fail gracefully per
step but report the chain as failed. At most 16 bindings; each gets its own
OS registration (Windows `RegisterHotKey`, X11 `XGrabKey`, macOS Carbon
hotkeys with unique ids), and a failed registration only warns. Invalid
`run` values are rejected at startup with the offending binding named.

## CLI triggers (`workflow`, `exec`)

Scripts and external tools drive the same engine without the daemon UI:

```bash
wilfred workflow review "Q3 report"  # run workflow on the top file hit
wilfred workflow review              # print the workflow's steps
wilfred exec copy_path+reveal C:/tmp/notes.md
wilfred exec media:play
wilfred exec workflow:review C:/tmp/notes.md
```

`workflow` resolves the target through normal search (top non-habit hit
with a path) and records history like `launch` does. `exec` runs any
action id directly on the target path, mirroring the `exec`/`action` IPC
command and `POST /exec` HTTP API.

## Window and layout workflow steps

Workflows and hotkeys can manage windows:

- `focus_window:<text>` focuses the first open window whose title or owner
  contains the text.
- `layout_apply:<name>` applies a saved window layout (see below).
- `window_minimize`, `window_maximize`, `window_restore`, `window_close`,
  `window_snap_left`, `window_snap_right` act on file-less window cards;
  in chains they apply to the chain target.

## Window layouts (`layout ...`)

`layout save <name>` snapshots every open window (match text + geometry +
maximized state) to `<data>/layouts/<name>.json`. `layout <name>` (or the
`Apply` action, or a `layout_apply:<name>` step) restores each entry onto
the first still-unplaced window whose title or owner contains the match
text. `layouts` lists, `layout delete <name>` removes. Layout names use
letters, digits, `_` and `-`; at most 64 windows per layout.

Under the hood this is the same native surface as the window actions:
Win32 `ShowWindow`/`SetWindowPos`/`WM_CLOSE` on the monitor work area,
EWMH/X11 client messages plus `XMoveResizeWindow` on Linux (X11 builds
only — Wayland-only reports unsupported), and Accessibility
minimize/zoom/close-button/position/size on macOS (needs Accessibility
permission, like window focus already did).
