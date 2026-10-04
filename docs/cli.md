# CLI Reference

`wilfred` is a single binary that acts as both the background daemon and a
one-shot CLI, dispatching on `argv[1]` (see `src/main.cpp`). All commands
share the same `wilfred::Service` boot sequence: load config, open the
index, load plugins.

```
wilfred                 Run the background daemon (overlay + indexer)
wilfred daemon           Same as above
wilfred search <query>   Search the local index and print results
wilfred launch <query>   Search and open the top result
wilfred workflow <name> [target]  Run a named workflow on a search target
wilfred exec <action> [target]    Run any result action directly
wilfred index            Scan configured roots and persist the index
wilfred status           Print index statistics
wilfred preview <path>   Print a file preview (text head, directory listing, or image info)
wilfred backup [path]    Write config/snippets/index archive
wilfred restore [path]   Restore from a backup archive
wilfred sync-push        Upload backup to sync.url
wilfred sync-pull        Download backup from sync.url
wilfred convert ...    Convert audio/image files (wav to mp3, bmp to png, ...)
wilfred bgremove ...   Remove image background to transparent PNG
wilfred import --list    List importable launchers (Alfred, Raycast, PowerToys, ...)
wilfred import --detect   Scan default locations for other launchers
wilfred import <id|auto> [--from <path>] [--dry-run] [--overwrite]
                         Import hotkey, web searches, snippets, ...
wilfred history-clear    Erase local search history
wilfred setup [--overwrite]  First-run wizard (roots, hotkey, browser)
wilfred config-validate  Validate wilfred.yml against the schema
wilfred config-open      Open wilfred.yml in the editor
wilfred config-path      Print the wilfred.yml path
wilfred config-get <key> Print one setting (section.key)
wilfred config-set <key> <value>  Update one setting (validated)
wilfred config-reset     Restore defaults (backs up first)
wilfred plugin <list|pending|install|approve|revoke>  Registry + trust
wilfred tile <preset>    Tile open windows (halves|thirds|grid|columns|rows|stack)
wilfred update [--check] Check for and install updates
wilfred help             Show the built-in help text
```

See [import.md](import.md) for the per-launcher mapping (macOS / Windows /
Linux) and merge semantics.

Running `wilfred` with an argument that isn't one of the above subcommands
treats the whole argument list as a search query — `wilfred firefox` is
shorthand for `wilfred search firefox`.

## `wilfred` / `wilfred daemon` / `wilfred run`

Starts the background daemon: opens/recovers the index, starts the
filesystem watcher, registers the global hotkey (default `Ctrl+Alt+W`,
`⌘+Alt+W` on macOS), starts the local IPC server (and the HTTP API if
`api.enabled`), creates the overlay window, and pumps the event loop until
quit. This is what should run continuously (e.g. as a login item / autostart
entry, or from the system tray).

On Windows, this build has no console: launching `wilfred.exe` directly
shows nothing, and the app is controlled via its tray icon.

## `wilfred search <query>`

```
wilfred search firefox
wilfred search "*.cpp in Projects"
wilfred search "25 * 42"
```

Runs a single query against the local index (talking to a running daemon
over the IPC socket when one is available, or opening the index directly
otherwise — see `Service::run_search`) and prints up to 40 results to
stdout. Useful for scripting or quick terminal lookups without opening the
overlay. Exit code is `0` on success; a missing query argument exits `2`.

## `wilfred launch <query>` / `wilfred open <query>`

Same search as above, but immediately executes the top result's default
action (open the file/app, open the URL, etc.) instead of printing results —
equivalent to typing a query in the overlay and pressing Enter without
looking at the list.

## `wilfred workflow <name> [target]`

Runs a named workflow from `workflows:` in `wilfred.yml` without the
overlay. With no target it prints the workflow's steps; with a target query
it resolves the top file hit (recording history like `launch` does) and
executes the chain on it:

```
wilfred workflow review
wilfred workflow review "Q3 report"
```

Unknown workflow names list the configured ones on stderr. See
[automation.md](automation.md).

## `wilfred exec <action> [target]`

Runs any result action id directly (`copy_path+reveal`, `workflow:review`,
`media:play`, `focus_window:code`, `layout_apply:work`, `tile:grid`,
`config:set:search.max_results=40`, ...), mirroring the `exec`/`action` IPC
command and the `POST /exec` HTTP API:

```
wilfred exec copy_path C:/tmp/notes.md
wilfred exec media:play
```

Exit code is `0` when the action reports success, `1` when it fails.

## `wilfred index`

Forces an immediate full scan of all configured index roots
(`index.paths`, or platform defaults when empty) and persists the result,
without waiting for the filesystem watcher's debounce or the periodic
rescan interval. Useful right after changing `index.paths`/`index.exclude`
in the config, or after a large batch of file changes made while the daemon
wasn't running.

## `wilfred status`

Prints `IndexStats` — file/directory/application counts, error count, total
indexed bytes, whether a scan is currently in progress, and the duration of
the last scan.

## `wilfred preview <path>`

Prints a bounded preview of a file or directory without opening it: kind,
size, and modification time, plus a text head for text files, a child
listing for directories, or image metadata for images. Same backend as the
overlay's `F3` preview pane, useful for scripts.

## `wilfred backup [path] [--no-index]`

Writes a single archive containing the current config, snippets, and (by
default) the full index to `path` (or a default location — see
`sync::default_backup_path()` — if omitted). Pass `--no-index` to back up
only config and snippets (much smaller, but a restore will need a fresh
index scan).

```
wilfred backup ~/wilfred-backup.tar
wilfred backup --no-index
```

## `wilfred convert <src> [--to <fmt>] [--out <dst>]`

Converts audio and image files. WAV to WAV (resample / remix / bit-depth)
and WAV to RAW plus BMP / PNG / PPM / TGA image conversion run natively
with no extra installs; compressed formats (mp3, ogg, opus, flac, m4a/aac,
wma, jpg, gif, webp, ...) transcode through an optional `ffmpeg` on PATH:

```
wilfred convert song.wav --to mp3
wilfred convert song.wav out.ogg
wilfred convert song.wav --to wav --rate 44100 --stereo --bits 16
wilfred convert photo.bmp --to png
wilfred convert photo.jpg out.png
```

With a bare format the output is written next to the source
(`song.mp3`, `photo.png`, `song 2.mp3` when taken). The same conversions
are available from the overlay as `convert <file> to <fmt>` (Enter converts
and reveals the output). No config keys are needed.

## `wilfred bgremove <src> [--out <dst>] [--tolerance N] [--color C]`

Removes the background of a PNG, BMP, PPM, or TGA image (JPG / GIF / WebP
via `ffmpeg` when installed) and writes a transparent PNG. The background
color is auto-sampled from the image corners unless `--color` is given;
`--tolerance 0-100` (default 32) controls how close a pixel must be, with
`--global` for chroma-keying every matching pixel instead of the default
border flood-fill, and `--feather 0-8` for edge smoothing:

```
wilfred bgremove photo.png
wilfred bgremove photo.png --tolerance 40 --color "#ffffff"
wilfred bgremove photo.png --out clean.png --global
```

Also available from the overlay as `bgremove <image> [tolerance] [#color]`.
Output is always PNG (alpha cannot survive in JPEG), defaulting to
`<stem>.transparent.png` next to the source.

## `wilfred restore <path>`

Restores config, snippets, and (if present in the archive) the index from a
previously created backup archive. This overwrites the current user config
and index directory — back up anything you want to keep first.

## `wilfred sync-push` / `wilfred sync-pull`

Uploads (`sync-push`) or downloads (`sync-pull`) a backup archive to/from
`sync.url` using `sync.token` for authentication, when `sync.enabled: true`.
See [sync-and-backup.md](sync-and-backup.md) for the archive format and
transport details. `sync.interval_seconds` (if non-zero) additionally
enables periodic auto-sync from within the running daemon; these CLI
commands trigger a sync on demand regardless of that interval.

## `wilfred import ...`

Imports hotkeys, custom web searches (as `macros:` + `quicklinks:`),
snippets, aliases, theme and the default search template from other
launchers — Alfred / Raycast (macOS), PowerToys Run / Flow Launcher / Wox /
Keypirinha / Listary (Windows), Ulauncher / Albert / KRunner / Rofi (Linux).
See [import.md](import.md) for the full mapping and examples:

```
wilfred import --list
wilfred import --detect
wilfred import auto --dry-run
wilfred import alfred --from Alfred.alfredpreferences --dry-run
wilfred import flowlauncher --from Settings.json --overwrite
```

Default is merge (existing values win); `--overwrite` replaces conflicts.
`--dry-run` previews without writing; a `.pre-import.bak` backup is written
before every real import.

## `wilfred history-clear` / `wilfred clear-history`

Loads, clears, and saves the local search/selection history file, removing
all recency/frequency/learned-choice data used by the ranker. Does not
affect the file index itself.

## `wilfred setup [--overwrite]`

First-run wizard: prompts for index roots, the global hotkey key, and the
browser search template, then writes only the missing keys back to
`wilfred.yml` (existing keys are kept unless `--overwrite`). Refuses to
write when the result fails schema validation. The overlay `setup` mini
reports the same three items as cards for users who prefer the GUI.

## `wilfred config-validate` / `config-open` / `config-path`

`config-validate` loads `wilfred.yml` with the same loader the daemon
uses and prints `valid:` or the human-readable error (exit `1`).
`config-open` opens the file in the editor, `config-path` prints its path.

## `wilfred config-get <key>` / `config-set <key> <value>` / `config-reset`

Typed single-setting access without hand-editing YAML (`section.key`,
e.g. `search.max_results`, `browser.search_template`; lists take
comma-separated values). `config-set` validates before writing and
refuses on schema errors; the daemon still needs a restart to pick the
change up. Same engine as the overlay `settings edit <key> <value>` /
`settings get <key>` cards. `config-reset` restores the shipped defaults,
saving the current file as `.pre-reset.bak` first.

## `wilfred plugin <list|pending|install|approve|revoke>`

Registry and trust management (see [plugins.md](plugins.md)):

```
wilfred plugin list              # registry index, or installed plugins
wilfred plugin pending           # plugins awaiting trust approval
wilfred plugin install <id>      # download + sha256-verify into plugins/<id>/
wilfred plugin approve <id>      # record hash + permissions (or --all)
wilfred plugin revoke <id>       # un-approve; queries skip it again
```

## `wilfred tile <preset>`

Tiles all open windows across the primary work area without the overlay:
`halves`, `thirds`, `grid`, `columns [N]`, `rows [N]`, `stack` (bare
`wilfred tile` defaults to `grid`). Same engine as `layout tile …` in the
overlay and the `tile:<preset>` workflow step. Fails with a clear error on
Wayland-only Linux and on mobile, where window management is unsupported.

## `wilfred update [--check]`

Checks the release feed and installs a newer build (`--check` only
reports, `--yes` skips the prompt).

## `wilfred help` / `wilfred -h` / `wilfred --help`

Prints the usage text shown above.

## Exit codes

`main()` catches any `std::exception` thrown out of a command, logs it
(`log_error`), prints `wilfred: <message>` to stderr, and exits `1`. Commands
that detect a usage error directly (e.g. `search` with no query) exit `2`
with a `usage: ...` message. Successful commands exit `0`.
