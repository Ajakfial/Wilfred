# Configuration Reference

Wilfred is configured with a single YAML file, created with default values
on first run (`load_or_create_user_config`, `include/wilfred/config/config.hpp`).
Invalid values produce a human-readable error at startup rather than a
crash or silent fallback.

## File location

| Platform | Config file | Index data directory |
|---|---|---|
| Windows | `%APPDATA%\Wilfred\wilfred.yml` | `%LOCALAPPDATA%\Wilfred\` |
| macOS | `~/Library/Application Support/Wilfred/wilfred.yml` | same directory |
| Linux | `~/.config/wilfred/wilfred.yml` | `~/.local/share/wilfred/` |

The shipped template with every key and its default value is
[`config/wilfred.default.yml`](../config/wilfred.default.yml) — copy it to
the path above and edit, or just edit the file Wilfred creates on first run.
There is no live-reload: restart the daemon (or re-run the CLI) after
editing.

## `search:` — query-time behavior

| Key | Default | Meaning |
|---|---|---|
| `include_system_files` | `false` | Whether the *index* includes OS/system files at all (separate from whether they're shown — see `show_system_in_results`) |
| `include_hidden_files` | `true` | Whether hidden files/dotfiles are searchable |
| `show_system_in_results` | `false` | Whether system files, if indexed, appear in results |
| `max_results` | `40` | Max results returned per query |
| `debounce_ms` | `12` | Delay before searching after each keystroke in the overlay |
| `web_search_fallback` | `true` | Offer a "search the web" result when nothing else matches well |
| `treat_urls_as_open` | `true` | A query that looks like a URL opens it directly instead of just fuzzy-matching it |
| `min_query_length` | `1` | Minimum characters before searching begins |
| `fuzzy` | `true` | Enable fuzzy (non-exact) name matching |
| `acronyms` | `true` | Enable acronym matching (e.g. `vsc` → Visual Studio Code) |
| `context_aware` | `true` | Factor recent folders/extensions/session tokens into ranking |
| `clipboard` | `true` | Enable clipboard as a search source/ranking signal |
| `minis` | `true` | Enable mini results (`weather`, `time`, `ram`, `emoji`, `fx`, ...) |
| `macros` | `true` | Enable macro expansion (`!yt`, `gh`, ...) |
| `snippets` | `true` | Enable text-expansion snippet matching |
| `plugins` | `true` | Enable querying loaded plugins |

## `index:` — what gets scanned and how

| Key | Default | Meaning |
|---|---|---|
| `paths` | `[]` | Roots to index. Empty means platform defaults (home directory, Applications/Start Menu locations, common data dirs) |
| `exclude` | `[node_modules, .git, .svn, .hg, __pycache__, .cache, Cache, caches, CMakeFiles, .Trash, $Recycle.Bin, System Volume Information]` | Directory names to skip entirely |
| `exclude_globs` | `[*.tmp, *.swp, *~]` | Glob patterns to skip |
| `system_directories` | `[]` | Extra paths treated as "system" (subject to `include_system_files`/`show_system_in_results`) beyond the platform's built-in list |
| `follow_symlinks` | `false` | Whether the walker follows symlinks (off by default to avoid cycles/duplication) |
| `index_hidden` | `true` | Whether the walker descends into hidden directories at all |
| `index_system` | `false` | Whether the walker descends into system directories at all |
| `max_file_size_bytes` | `0` | Skip files larger than this for indexing (`0` = no limit) |
| `content_indexing` | `true` | Enable indexing file *contents* (not just names) for eligible text/source/config files |
| `content_max_bytes` | `131072` | Max bytes read per file for content indexing |
| `content_max_tokens` | `480` | Max tokens extracted per file for content indexing |
| `workers` | `0` | Indexer thread count (`0` = auto-detect from CPU count) |
| `cpu_percent_limit` | `45` | Soft cap on indexer CPU usage, to stay unobtrusive while running in the background |
| `memory_limit_mb` | `384` | Soft memory budget for the indexer |
| `batch_size` | `2048` | Records processed per batch during a scan |
| `debounce_fs_ms` | `80` | Debounce window for filesystem-watcher events before re-indexing |
| `rescan_interval_seconds` | `0` | Periodic full rescan interval (`0` = rely on the watcher only, never force-rescan) |
| `persist_every_records` | `50000` | Snapshot the index to disk after this many changed records, in addition to periodic checkpoints |
| `wal_compact_bytes` | `8388608` (8 MiB) | Compact the write-ahead log once it grows past this size |
| `extensions.include` | `[]` | If non-empty, *only* these extensions are indexed |
| `extensions.exclude` | `[.tmp, .temp, .part, .crdownload, .bak, .cache]` | Extensions never indexed |

See [indexing.md](indexing.md) for how these interact with the walker,
watcher, and write-ahead log.

## `ranking:` — scoring weights

Every key is an integer weight fed into `rank_record()`
(`include/wilfred/search/rank.hpp`) — higher means that signal contributes
more to a result's final score. There's no absolute scale to hit; they only
matter relative to each other. Defaults, roughly ordered by default
magnitude:

| Key | Default | Signal |
|---|---|---|
| `exact_name` | 1200 | Query exactly matches the file/app name |
| `learned_choice` | 520 | This query previously led to choosing this exact result |
| `alias` | 500 | Query matched a configured `aliases` entry |
| `prefix_name` | 700 | Name starts with the query |
| `acronym` | 640 | Query matches the name's acronym (first letters of tokens) |
| `previous_selection` | 400 | This path was previously selected for *some* query (weaker than `learned_choice`) |
| `substring_name` | 420 | Query appears anywhere in the name |
| `application` | 350 | Result is a discovered application |
| `frequency` | 260 | How often this path has been selected historically |
| `fuzzy_name` | 280 | Fuzzy/subsequence match on the name |
| `clipboard_overlap` | 240 | Overlaps with current clipboard content |
| `recency` | 220 | How recently this path was modified |
| `context_parent` | 200 | Same parent folder as recent selections |
| `content_hit` | 190 | Query matched indexed file *content*, not just the name |
| `path_component` | 180 | Query matches a path segment above the file name |
| `word_boundary` | 160 | Match starts at a word boundary within the name |
| `access_recency` | 150 | How recently this path was *accessed* (vs. modified) |
| `token_proximity` | 140 | Multiple query tokens found close together |
| `context_extension` | 90 | Same extension as recent selections |
| `extension` | 90 | Extension itself is a relevant signal (e.g. matches `type:` filter intent) |
| `hour_affinity` | 70 | This path is typically chosen around the current hour of day |
| `directory_bonus` | 40 | Small bonus for directories, to keep folders visible among many file hits |

Additional custom scoring can be registered at runtime without adding a new
config key — see `RankPipeline` in [ranking.md](ranking.md).

## `aliases:` — query shortcuts

Maps a typed word to a target the search engine should prefer, weighted by
`ranking.alias`. Defaults:

```yaml
aliases:
  browser: default-browser
  code: Visual Studio Code
  vscode: Visual Studio Code
  explorer: file-manager
```

Add your own, e.g. `aliases: { term: Windows Terminal }`.

## `macros:` — query templates

Maps a macro name to a URL template. `{query}` and `{query_enc}` are
replaced with the raw/URL-encoded remainder of the query; `{clipboard}` and
`{clipboard_enc}` are replaced with the current clipboard contents. Invoked
as `!name argument` or `name:argument` (see
[query-language.md](query-language.md)). Built-in defaults:

```yaml
macros:
  yt: "https://www.youtube.com/results?search_query={query}"
  gh: "https://github.com/search?q={query}"
```

User-defined macros in this section are merged with the built-ins
(`merged_macros()`); a user key with the same name overrides the built-in.

## `scopes:` — named directory groups

Named sets of directories referenced in a query as `scope:<name>` (see
`apply_named_scopes()` in `search/filter.hpp`). Default only defines an
empty `home` scope as an example:

```yaml
scopes:
  home: []
  projects:
    - ~/Projects
    - ~/Code
```

## `custom_metadata:`

An open key/value map (`std::unordered_map<std::string, std::string>`, empty
by default) with no built-in meaning — intended for plugins or a custom
`RankPipeline` signal to read project-specific tags out of the config
without Wilfred needing to know about them.

## `history:`

| Key | Default | Meaning |
|---|---|---|
| `enabled` | `true` | Whether search/selection history is recorded at all |
| `max_entries` | `8000` | Cap on stored history entries (oldest pruned first) |
| `persist` | `true` | Whether history is saved to disk between runs (vs. in-memory only for the current session) |

## `hotkey:`

| Key | Default | Meaning |
|---|---|---|
| `enabled` | `true` | Whether the global hotkey is registered at all |
| `modifiers` | `[ctrl, alt]` | Modifier keys |
| `key` | `W` | The key itself |
| `use_command_on_macos` | `true` | On macOS, remap `ctrl` in `modifiers` to Command, so the same config produces `⌘+Alt+W` there and `Ctrl+Alt+W` elsewhere |

## `browser:`

| Key | Default | Meaning |
|---|---|---|
| `provider` | `auto` | Which browser to use for opening URLs/web searches (`auto` detects the OS default; see `browser::list_browsers()` for named alternatives) |
| `search_template` | `https://www.google.com/search?q={query}` | URL template for the web-search fallback and `?`/`g` query prefix |

## `logging:`

| Key | Default | Meaning |
|---|---|---|
| `level` | `info` | Log verbosity |
| `file` | `""` | Log file path; empty means stderr only |
| `max_file_bytes` | `2097152` (2 MiB) | Rotate/truncate the log file past this size |

## `ui:`

| Key | Default | Meaning |
|---|---|---|
| `theme` | `dark` | Overlay color theme |
| `max_visible` | `9` | Max results shown in the overlay list at once (scrollable beyond that; independent of `search.max_results`, which caps what the engine computes) |
| `width` | `720` | Overlay window width in pixels |

## `plugins:`

| Key | Default | Meaning |
|---|---|---|
| `enabled` | `true` | Whether the plugin host loads plugins at all |
| `directories` | `[]` | Plugin search directories; empty means the platform defaults (`<config dir>/plugins`, `<data dir>/plugins`) |
| `timeout_ms` | `400` | Timeout for a single plugin query (native call or stdio round-trip) before it's skipped for that search |

See [plugins.md](plugins.md) for the manifest format plugins are discovered
by.

## `api:` — optional local HTTP API

| Key | Default | Meaning |
|---|---|---|
| `enabled` | `false` | Whether the local HTTP API starts at all |
| `bind` | `127.0.0.1` | Bind address — leave this as loopback unless you specifically intend to expose the API beyond the local machine, and understand the security implications of doing so (see [SECURITY.md](../SECURITY.md)) |
| `port` | `17380` | TCP port |
| `token` | `""` | Bearer token required on every request; set this before enabling the API |

See [ipc-and-api.md](ipc-and-api.md) for the request/response format.

## `sync:`

| Key | Default | Meaning |
|---|---|---|
| `enabled` | `false` | Whether `sync-push`/`sync-pull` (and periodic auto-sync) are permitted |
| `url` | `""` | Remote endpoint for the backup archive |
| `token` | `""` | Auth token sent with sync requests |
| `interval_seconds` | `0` | If non-zero, the daemon syncs automatically on this interval |
| `include_index` | `true` | Whether the index is part of what's synced (a large payload) vs. config/snippets only |

See [sync-and-backup.md](sync-and-backup.md).

## `snippets:`

| Key | Default | Meaning |
|---|---|---|
| `expansion` | `true` | Enable text-expansion snippet matching in search |
| `prefix` | `;` | Trigger prefix recognized in queries |
| `auto_paste` | `false` | Automatically simulate a paste of the expanded text on selection, instead of just copying it to the clipboard |
| `items` | `{}` | Inline snippet definitions (trigger → body); snippets can also be added/edited at runtime and are persisted to their own store — see `SnippetStore` in `include/wilfred/search/snippets.hpp` |

## Validation and errors

`load_config_file()` / `load_config_text()` return `false` and populate a
`ConfigError{message}` on any parse or validation failure — Wilfred surfaces
this as a startup error rather than falling back to partial defaults
silently. Common causes: malformed YAML, a ranking weight or numeric field
that isn't an integer, or an unrecognized top-level key type mismatch (e.g.
a scalar where a list is expected).
