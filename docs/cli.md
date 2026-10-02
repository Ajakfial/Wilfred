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
wilfred import --list    List importable launchers (Alfred, Raycast, PowerToys, ...)
wilfred import --detect   Scan default locations for other launchers
wilfred import <id|auto> [--from <path>] [--dry-run] [--overwrite]
                         Import hotkey, web searches, snippets, ...
wilfred history-clear    Erase local search history
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
`media:play`, `focus_window:code`, `layout_apply:work`, ...), mirroring the
`exec`/`action` IPC command and the `POST /exec` HTTP API:

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

## `wilfred help` / `wilfred -h` / `wilfred --help`

Prints the usage text shown above.

## Exit codes

`main()` catches any `std::exception` thrown out of a command, logs it
(`log_error`), prints `wilfred: <message>` to stderr, and exits `1`. Commands
that detect a usage error directly (e.g. `search` with no query) exit `2`
with a `usage: ...` message. Successful commands exit `0`.
