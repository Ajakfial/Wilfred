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

## Editing without hand-editing YAML

Scalar and list keys can be read and written in place, validated against
the same schema the daemon uses:

- Overlay: `settings get <section.key>`, `settings edit <section.key>
  <value>` (also `config set/get`), `settings reset` for defaults.
- CLI: `wilfred config-get <key>`, `wilfred config-set <key> <value>`
  (lists take comma-separated values), `wilfred config-reset` (backs up
  to `.pre-reset.bak` first), `wilfred setup` for the first-run wizard.

`config-set` refuses to write when the result fails validation, and
workflows/quicklinks/macros/aliases (nested maps) stay file-edited —
the editor covers the ~70 scalar/list keys, not the whole schema.

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
| `usn_scan` | `true` | Windows only: enumerate NTFS volumes via the MFT/USN journal for fast full-disk scans (needs elevation; falls back to directory walk otherwise) |
| `max_file_size_bytes` | `0` | Skip files larger than this for indexing (`0` = no limit) |
| `content_indexing` | `true` | Enable indexing file *contents* (not just names) for eligible text/source/config files and real documents (`.pdf`, `.docx`, `.xlsx`, `.pptx`, `.odt`, `.rtf`, `.html`) |
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

## `clipboard:`

| Key | Default | Meaning |
|---|---|---|
| `manager` | `true` | Whether clipboard texts are kept in a persistent, searchable history (`clips`) |
| `max_entries` | `200` | Cap on unpinned clips (pinned clips always survive; oldest unpinned pruned first) |
| `persist` | `true` | Whether clips are saved to disk between runs |

## `hotkey:`

| Key | Default | Meaning |
|---|---|---|
| `enabled` | `true` | Whether the global hotkey is registered at all |
| `modifiers` | `[ctrl, alt]` | Modifier keys |
| `key` | `W` | The key itself |
| `use_command_on_macos` | `true` | On macOS, remap `ctrl` in `modifiers` to Command, so the same config produces `⌘+Alt+W` there and `Ctrl+Alt+W` elsewhere |

## `hotkeys:` — extra system-wide bindings

A mapping of binding name to `{modifiers, key, run}`, fired from any app
without opening the overlay. `run` is one of: `show` (toggle the overlay),
`macro:<text>` (open a macro/quicklink URL, clipboard feeds `{clipboard}`
placeholders), `system:<id>` (`lock`, `sleep`, ...), `media:<id>`
(`play`, `mute`, ...), or `workflow:<name>` (runs against the clipboard —
use targetless steps). At most 16 bindings; each gets its own OS
registration, and a failed one only warns. See
[automation.md](automation.md).

```yaml
hotkeys:
  google-clip:
    modifiers: [ctrl, alt]
    key: G
    run: macro:gclip
```

## `browser:`

| Key | Default | Meaning |
|---|---|---|
| `provider` | `auto` | Which browser to use for opening URLs/web searches (`auto` detects the OS default; see `browser::list_browsers()` for named alternatives) |
| `search_template` | `https://www.google.com/search?q={query}` | URL template for the web-search fallback and `?`/`g` query prefix |
| `library` | `true` | Whether bookmarks, recent history, and open tabs are searchable (as a provider plus the `bm` mini) |

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
| `accent` | `""` | Custom accent (`#RRGGBB`, `#RGB`, or `indigo/blue/green/teal/pink/orange/red/purple`); empty = default |
| `font_size` | `0` | Base overlay font size in px (`0` = default 14, clamped 10–24) |
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

## `providers:`

| Key | Default | Meaning |
|---|---|---|
| `semantic` | `false` | Whether the soft-match provider runs on every query (vector HNSW + trigram fallback, see below) |
| `semantic_min_score` | `0.3` | Minimum similarity (0–1) for a semantic hit (applies to both backends) |
| `semantic_backend` | `hybrid` | `vector` (HNSW only), `trigram` (legacy soft-match only), or `hybrid` (both, merged) |
| `semantic_max_results` | `10` | Cap on semantic hits per query |

## `embedding:` — local vector model for semantic search

Works offline via the built-in hash embedder; point at llama.cpp for higher
quality. Vectors are stored as `vectors.bin` (HNSW) next to
`snapshot.wilf`/`journal.wal` and rebuilt automatically.

| Key | Default | Meaning |
|---|---|---|
| `enabled` | `false` | Enable the vector sidecar (also enabled implicitly when `providers.semantic` is on) |
| `backend` | `auto` | `auto` (server → model → hash), `hash`, `server`, or `llamacpp` |
| `model` | `""` | Path to a local `.gguf` model for llama.cpp |
| `endpoint` | `""` | llama.cpp server endpoint, e.g. `http://127.0.0.1:8080/embedding` (run `llama-server -m model.gguf --embedding`) |
| `dim` | `384` | Embedding dimension (32–4096) |
| `min_score` | `0.45` | Minimum cosine for a vector hit |
| `max_results` | `10` | Cap on vector hits per query |

## `ai:` — optional assistant (OpenAI / Anthropic / Gemini / Groq)

Disabled until you add a key. Query with `ai <question>` or `ask <question>`.

| Key | Default | Meaning |
|---|---|---|
| `enabled` | `false` | Whether `ai ...` queries hit the network at all |
| `provider` | `auto` | `auto`, `openai`, `anthropic`, `gemini`, or `groq` |
| `model` | `""` | Model name (defaults per provider: `gpt-4o-mini`, `claude-3-5-sonnet-latest`, `gemini-1.5-flash`, `llama-3.1-8b-instant`) |
| `api_key` | `""` | Your API key (never logged) |
| `endpoint` | `""` | Override URL (for proxies / self-hosted OpenAI-compatible servers) |
| `max_tokens` | `1024` | Max response tokens |
| `temperature` | `0.7` | Sampling temperature 0–2 |
| `timeout_ms` | `30000` | Request timeout |

## `sources:` — calendar / contacts / notes + OCR

| Key | Default | Meaning |
|---|---|---|
| `calendar` | `true` | Search `.ics` events (VEVENT summary/date) |
| `contacts` | `true` | Search `.vcf` contacts (name/email/phone) |
| `notes` | `true` | Search Markdown/text notes (`.md`/`.txt`/`.org`) |
| `calendar_paths` | `[]` | Extra roots to scan for `.ics` (defaults cover Thunderbird/Evolution/Apple/Outlook exports) |
| `contacts_paths` | `[]` | Extra roots to scan for `.vcf` |
| `notes_paths` | `[]` | Extra roots to scan for notes (defaults cover `~/Notes`, `~/Documents/Notes`) |
| `ocr` | `false` | OCR images during indexing via the `tesseract` CLI (`--psm 6`, `ocr_languages`) |
| `ocr_languages` | `eng` | Tesseract `-l` value |
| `max_results` | `8` | Cap per source per query |

## `remotes:` — opt-in remote search backends

Off by default. No network calls unless `enabled: true` **and** at least one
`source` is listed. Each source is a GET template with `{query}` /
`{query_enc}` placeholders; the endpoint returns
`{"results":[{"title":"..","subtitle":"..","url":"..","score":500}]}` (a bare
array works too). Results appear as `remote` cards after file search; failures
are silent per-source (no blocking). Use a token in the URL query when the
server needs auth.

```yaml
remotes:
  enabled: true
  timeout_ms: 5000
  max_results: 8
  sources:
    - name: wiki
      url: "https://wiki.example.com/api/search?q={query_enc}"
      max_results: 5
      headers:
        Authorization: "Bearer <token>"
```

| Key | Default | Meaning |
|---|---|---|
| `enabled` | `false` | Master switch; `false` disables all remote calls |
| `timeout_ms` | `5000` | Per-source fetch timeout (1000–30000) |
| `max_results` | `8` | Cap across all remote sources per query (1–50) |
| `sources` | `[]` | List of `{name, url}` (or `name: url` map); `url` must start with `http(s)://` |
| `max_results` per source | `0` | Per-source cap (0 = use global `max_results`); list form only |
| `headers` per source | `{}` | Extra request headers, e.g. `Authorization: Bearer <token>` (list form only; never logged) |

## `packages:` — system package-manager search (desktop)

On by default, but fires only on explicit `<manager> <query>`
invocations (`winget firefox`, `brew switch`, `apt vlc`) — plain file
searches never touch a package manager, so there is no per-keystroke
cost and no surprise network traffic. Each hit installs on Enter
(`winget`/`brew`/`choco`/`flatpak` run headless; `apt`/`pacman` use
`polkit`, falling back to a copy-pasteable `sudo` command); `Tab` copies
the shell install command instead. Type `packages` for the detected
manager list.

```yaml
packages:
  enabled: true
  max_results: 8
  timeout_ms: 8000
  managers: []  # empty = per-OS defaults (Windows: winget/choco, macOS: brew)
```

| Key | Default | Meaning |
|---|---|---|
| `enabled` | `true` | Master switch |
| `max_results` | `8` | Cap per query (1–20) |
| `timeout_ms` | `8000` | Per-tool search timeout, hard kill past it (2000–30000) |
| `managers` | `[]` | Allowed ids from `winget, brew, apt, choco, flatpak, pacman`; empty = per-OS defaults |

## `layouts:` — saved layouts, tiling, monitor auto-apply (desktop)

| Key | Default | Meaning |
|---|---|---|
| `auto_apply` | `false` | Daemon polls the monitor signature (~5s) and applies a layout on change |
| `auto_layout` | `""` | Fallback layout when no `monitor_layouts` entry matches the new signature |
| `monitor_layouts` | `{}` | Signature-substring → layout name (e.g. `docked: work`) |

Tiling needs no config: `layout tile halves|thirds|grid|columns N|rows N|stack`
(or `tile …`, `wilfred tile …`) tiles current windows across the primary work
area immediately.

## `plugins:` — registry + trust-on-first-use

| Key | Default | Meaning |
|---|---|---|
| `registry` | `""` | Registry index URL for `wilfred plugin list/install` (empty = disabled) |
| `require_approval` | `true` | New/changed plugins show as `plugins` approval cards and are skipped until `wilfred plugin approve <id>` |

## `transcription:` — speech-to-text for audio

On-demand transcription of MP3s and MP4 audio (`transcribe <file>` in the
overlay) plus microphone dictation (`dictate [seconds]`). Same philosophy as OCR: the core stays dependency-free and shells
out to optional CLIs — `whisper.cpp` (`whisper-cli`) for recognition, plus
`ffmpeg` only to extract audio from containers (mp4/m4a/...). WAV files need
neither conversion nor ffmpeg. Nothing runs at query time: the mini resolves
the file and Enter transcribes, delivering the transcript to the clipboard
(plus an optional `<audio>.txt` sidecar, which then gets content-indexed on
rescan).

| Key | Default | Meaning |
|---|---|---|
| `enabled` | `true` | Whether `transcribe ...` queries run at all |
| `binary` | `""` | Explicit whisper binary path or command name; empty probes PATH for `whisper-cli`, then `whisper` |
| `model` | `""` | Explicit whisper model file (`.bin`); empty probes `<data>/models` for a ggml model |
| `language` | `"auto"` | Language hint passed as whisper `-l` (`en`, `de`, ...); `auto` leaves detection to the model |
| `save_txt` | `true` | Write `<audio>.txt` next to the source on success |
| `mic` | `""` | Microphone device for `dictate` (ffmpeg syntax for this OS); empty means the OS default |

Setup per OS: Windows — a `whisper.cpp` release build plus `winget install
ffmpeg`; macOS — `brew install whisper-cpp ffmpeg`; Linux — build
[whisper.cpp](https://github.com/ggerganov/whisper.cpp) plus distro
`ffmpeg`. Models (e.g. `ggml-base.en.bin`) come from
`huggingface.co/ggerganov/whisper.cpp`. Missing pieces surface as cards
with the exact install step instead of failing silently.

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
| `encrypt` | `false` | Encrypt the archive (SHA-256 KDF + keystream XOR, no new deps). Needs `password` or `key_file` |
| `password` | `""` | Sync encryption password (never logged). Prefer `key_file` on shared machines |
| `key_file` | `""` | Path to a file holding the password (trailing newlines trimmed) |

See [sync-and-backup.md](sync-and-backup.md).

## `snippets:`

| Key | Default | Meaning |
|---|---|---|
| `expansion` | `true` | Enable text-expansion snippet matching in search |
| `prefix` | `;` | Trigger prefix recognized in queries |
| `auto_paste` | `false` | Automatically simulate a paste of the expanded text on selection, instead of just copying it to the clipboard |
| `global_expansion` | `false` | Expand abbreviations typed in any app (Windows hook; overlay paste everywhere) |
| `items` | `{}` | Inline snippet definitions (trigger → body); snippets can also be added/edited at runtime and are persisted to their own store — see `SnippetStore` in `include/wilfred/search/snippets.hpp` |

Snippet bodies support `{date} {time} {datetime} {year} {month} {day}
{clipboard} {query}` placeholders. Snippets can carry a `folder:` in
`snippets.yml` and are filtered with `;folder/name` or `snip folder/name`.

## `pins:` — pinned favorites

Lowercased path/title substrings, always boosted by `ranking.pinned` (default
900). Managed live via `pin <text>`, `unpin <text>`, `pins` (persisted to
`pins.bin`), or the `pin_add`/`pin_remove` file actions. `favorites:` is an
accepted alias for `pins:`.

```yaml
pins:
  - firefox
  - /home/user/docs
```

## `workflows:` — named multi-step actions

Maps a workflow name to an ordered list of result actions run in order.
Each step is an action id accepted by `execute_result_action`
(`open`, `reveal`, `copy_path`, `copy_text`, `open_terminal`, ...),
or a `workflow:` reference. Query with `workflow <name>` to list,
`workflow <name>` / `run <name>` to run, or pick `Run <name>` from a
file's `Ctrl`/`⌘`+`K` actions. Ad-hoc chaining already works without
config via `+` (e.g. action id `copy_path+reveal`).

```yaml
workflows:
  review: [copy_path, reveal]
  ship: "copy_path+reveal"
  docs_review:
    steps: [copy_path, reveal]
```

## `quicklinks:` — parameterized quicklinks

Maps a name to a URL/path/command template with placeholders:
`{query}` `{query_enc}` `{clipboard}` `{clipboard_enc}` plus positional
`{1}` `{2}` ... and `{*}` (all args). Query with `ql <name> <args>`,
`!name args`, or `name:args` (bare `name args` also works when the name
is a known quicklink).

```yaml
quicklinks:
  docs: "https://example.com/search?q={query}"
  ticket: "https://example.com/t/{1}"
```

## `app_actions:` — per-app context actions

Maps an app-name substring (case-insensitive, matched against the
result title + path) to extra action ids appended to that app's
file/app results.

```yaml
app_actions:
  code: [open_terminal, open_editor, copy_path]
```

## Validation and errors

`load_config_file()` / `load_config_text()` return `false` and populate a
`ConfigError{message}` on any parse or validation failure — Wilfred surfaces
this as a startup error rather than falling back to partial defaults
silently. Errors name the file, the offending `section.key`, what was
got vs. expected, and an example; unknown keys suggest the closest
valid key (`Did you mean 'max_results'?`) and list all valid keys.
Common causes: malformed YAML, a ranking weight or numeric field
that isn't an integer, or an unrecognized top-level key type mismatch (e.g.
a scalar where a list is expected).
