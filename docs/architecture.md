# Architecture

Wilfred is a single C++20 binary (`wilfred`) that can run as a background
daemon (indexer + global hotkey + overlay window) or be invoked as a
one-shot CLI tool (`wilfred search ...`). This document walks through the
modules under `src/` / `include/wilfred/`, how they depend on each other, and
the life of a query from keypress to launched result.

## Module map

```
include/wilfred/
├── core/       shared low-level utilities (no dependencies on other modules)
├── config/     YAML config parsing + the Config struct
├── index/      on-disk index: records, string pool, WAL, engine
├── fs/         filesystem walking, watching, classification, volumes
├── search/     fuzzy matching, ranking, filters, macros, minis, engine
├── query/      classifies raw query text, drives the search engine
├── apps/       platform application discovery + launch/reveal/open
├── browser/    default-browser detection and web search URLs
├── history/    local search/selection history used as a ranking signal
├── hotkey/     global hotkey registration
├── plugin/     native (.dll/.so/.dylib) and stdio plugin hosting
├── providers/  extension point for additional search backends
├── ipc/        local control-socket protocol + optional local HTTP API
├── sync/       backup archive format + remote sync
├── ui/         overlay window glue (native windowing + embedded web UI)
├── service/    top-level daemon/CLI orchestration (wires everything together)
└── platform/   OS abstraction layer (native.hpp / platform.hpp)
```

Each `include/wilfred/<module>/*.hpp` has a matching `src/<module>/*.cpp`.
Dependencies flow roughly top-to-bottom in the list above: `core` has no
internal dependencies, `config` depends only on `core`, `index` depends on
`config` and `core`, and so on up to `service`, which depends on nearly
everything. `platform` sits below all of it as the only place `#ifdef`s for
Windows/macOS/Linux are allowed — see [Platform layer](#platform-layer).

## Startup and the `Service` class

Everything is orchestrated by `wilfred::Service`
(`include/wilfred/service/service.hpp`). `main.cpp` parses `argv[1]` as a
subcommand (`daemon`, `search`, `index`, `status`, `backup`, `restore`,
`sync-push`, `sync-pull`, `history-clear`, or a bare query treated as
`search`) and calls the matching `Service::run_*` method. See
[cli.md](cli.md) for the full command reference.

`Service` owns one instance of each long-lived subsystem:

```
Config          cfg_          — loaded once at boot
IndexEngine     index_        — the on-disk index (records + WAL)
SearchEngine    search_       — fuzzy match + ranking over the index
SnippetStore    snippets_     — text-expansion snippets
PluginHost      plugins_      — loaded native/stdio plugins
QueryInterpreter interpreter_ — classifies + dispatches a raw query string
HistoryStore    history_      — recency/frequency signals
FsWatcher       watcher_      — filesystem change notifications (daemon only)
GlobalHotkey    hotkey_       — OS hotkey registration (daemon only)
IpcServer       ipc_          — local socket for the CLI to talk to the daemon
HttpApiServer   http_         — optional localhost HTTP API
OverlayUi       ui_           — the search popup window (daemon only)
```

`Service::boot()` loads the config (`load_or_create_user_config`, writing
defaults on first run — see [configuration.md](configuration.md)), opens the
index directory, starts the plugin host, and constructs the
`QueryInterpreter`. `run_daemon()` additionally starts the filesystem
watcher, the global hotkey, the IPC server, and (if `api.enabled`) the HTTP
API, then creates and pumps the overlay window until quit.

## The life of a query

1. **Input.** Either the overlay UI captures a keystroke and calls back
   into C++ (see [UI layer](#ui-layer)), or the CLI passes `argv` straight to
   `Service::run_search`.
2. **Classification.** `classify_query()` (`query/classify.hpp`) looks at
   the raw text and buckets it into a `QueryKind` — `FileSearch`,
   `AppLaunch`, `Math`, `Url`, `WebSearch`, `FilteredSearch`, `Mini`,
   `Macro`, `Clipboard`, `Command`, `Alias`, `SystemAction`, or `Empty` —
   using cheap heuristics (`looks_like_url`, `looks_like_math`, macro/mini
   prefixes, filter syntax). See [query-language.md](query-language.md) for
   the full grammar.
3. **Interpretation.** `QueryInterpreter::interpret()`
   (`query/interpreter.hpp`) takes the classification and produces an
   `InterpretedQuery` (the classification plus a `vector<SearchResult>`). For
   most classes this means calling into `SearchEngine::search()`; for
   `Math` it calls `evaluate_math()`; for `Mini` it calls `mini_results()`;
   for `Macro` it calls `macro_results()`; for provider-backed queries it
   calls `ProviderRegistry::query_all()`; for plugin-backed queries it calls
   `PluginHost::query()`.
4. **Search + ranking.** `SearchEngine::search()`
   (`search/engine.hpp`) parses any filter clauses out of the query
   (`parse_filter_clauses`), builds a `RankContext` from history, clipboard,
   config, and time-of-day (`fill_rank_context`, `search/context.hpp`),
   scans the index store applying `record_matches_filter`, and scores each
   candidate with `rank_record()` plus any registered `RankPipeline` extra
   signals. Results are capped with `take_top()`. See
   [ranking.md](ranking.md) for exactly how scores are computed, and
   [indexing.md](indexing.md) for what's actually being scanned.
5. **Presentation.** Each `SearchResult` carries a `title`, `subtitle`,
   `path`, `kind`, `score`, a primary `ResultAction`, and optionally extra
   `ResultActionItem`s (e.g. a plugin offering "Open" and "Copy path"). The
   overlay serializes these to JSON (`overlay_results_json`,
   `ui/overlay.hpp`) for the embedded web UI to render; the CLI prints them
   as text.
6. **Execution.** Selecting a result calls `execute_result()` /
   `execute_result_action()` (`query/interpreter.hpp`,
   `search/actions.hpp`), which dispatches on `ResultAction` — `Open` calls
   `launch_path()` (`apps/discovery.hpp`), `Reveal` shows the file in the
   OS file manager, `Copy` writes to the clipboard, `WebSearch`/`Url` open
   the default browser, `Plugin` calls back into `PluginHost::execute()`,
   and so on. The result of a selection is also fed back into
   `HistoryStore::record_choice()` so future searches for the same query
   rank the chosen result higher (`ranking.learned_choice` /
   `previous_selection`).

## Index layer

The index is Wilfred's core data structure: a compact, memory-mapped store
of every indexed file/folder/application, kept in sync with the filesystem
via a write-ahead log. See [indexing.md](indexing.md) for the full design —
in short:

* `fs::walk_tree()` walks configured roots, applying excludes
  (`path_is_excluded`) and classifying each entry (`classify_extension`,
  `classify_path`) into a `FileKind`.
* `apps::index_applications()` separately discovers installed applications
  per-platform (Start Menu shortcuts, `.app` bundles, `.desktop` files) and
  feeds them into the same index as `FileKind::Application` records.
* Every insert/update/delete/rename goes through `IndexEngine`, which
  appends a `WalEntry` to the `WriteAheadLog` before mutating the in-memory
  `IndexStore`, and periodically calls `persist_snapshot()` /
  `compact()` to fold the WAL back into a durable snapshot. On startup,
  `recover()` replays any WAL entries left over from an unclean shutdown.
* `FsWatcher` (`fs/watcher.hpp`) uses `ReadDirectoryChangesW` on Windows,
  `FSEvents` on macOS, and `inotify` on Linux to push live change events
  back into `IndexEngine::upsert_file` / `remove_path` / `rename_path`,
  with a debounce and a periodic full rescan as a fallback.

## Search layer

`search/` contains the query-time logic that runs over the index:

* `fuzzy.hpp` — subsequence/fuzzy scoring (`score_fuzzy`), acronym matching,
  and a bounded Levenshtein distance used for near-miss suggestions.
* `filter.hpp` — parses inline filter syntax (`*.cpp in Projects`,
  `type:image`, `size:>10mb`) out of the query text into a `SearchFilter`,
  and matches records against it.
* `rank.hpp` — combines many independent signals (exact/prefix/fuzzy name
  match, path component match, extension, recency, frequency, alias,
  learned choice, clipboard overlap, content hit, hour-of-day affinity...)
  into a single integer score using the weights in `RankingWeights`
  (configurable — see [configuration.md](configuration.md)). Extra signals
  can be registered at runtime via `RankPipeline` /
  `default_rank_pipeline()` without touching `rank_record` itself.
* `context.hpp` — builds the `RankContext` fed into ranking: recent
  parent folders/extensions/names, session tokens, clipboard tokens, and
  the current hour/weekday.
* `minis.hpp` — recognizes a fixed set of system "mini" queries (`weather`,
  `time`, `disk`, `ram`, `cpu`, `process <name>`, `clip[s]`, `battery`,
  `uptime`, `speedtest`, `screenshot [fullscreen|window|region]`, `emoji`, `symbol`, `fx`,
  `uuid`/`base64`/`sha256`/`lorem`/`json`,
  `lock`/`sleep`/`shutdown`/`restart`/`logout`, …) and returns synthetic
  `SearchResult`s rather than index hits. Glyph catalogs live in
  `glyphs.hpp`; currency conversion is shared with the calculator
  (`math/expr.hpp`); system actions call `native_system_action()` and
  screenshots call `native_take_screenshot()` (`search/screenshot.hpp`).
* `macros.hpp` — user- and built-in-defined query templates (`!yt`, `gh`)
  that expand `{query}` / `{clipboard}` placeholders into a URL.
* `content.hpp` — decides which files are eligible for content indexing and
  tokenizes their text (bounded by `content_max_bytes` /
  `content_max_tokens` in config) so `content:` filters and the
  `content_hit` ranking signal can work.
* `snippets.hpp` — a small text-expansion store (trigger → body), matched
  and inserted separately from the main index.
* `clipboard.hpp` — reads the OS clipboard and clipboard history, used both
  as a "mini" result source and as ranking context (`clipboard_overlap`).
* `actions.hpp` — turns a `SearchResult` + chosen `ResultActionItem` into
  an actual OS action, and lets plugins register their own actions. File
  and folder results expose rich actions (`search/file_ops.hpp` for the
  portable pieces: POSIX/`file://`/WSL path flavors, SHA-256 hashing,
  stored-zip creation, new file/folder): open, reveal, copy flavors, hash,
  compress, terminal/editor here, new file/folder, and per-file Open With
  entries from `native_apps_for_file()` / `native_open_with()`.

## Extensibility: plugins and providers

Two different extension points exist:

* **`providers::SearchProvider`** — an in-process C++ interface
  (`providers/provider.hpp`) for embedding another search backend directly
  into the binary. `ProviderRegistry::query_all()` fans a query out to every
  registered provider and merges the results. This is a compile-time
  extension point (you add a provider by writing C++ and registering it),
  not something end users configure.
* **`plugin::PluginHost`** — out-of-process/dynamically-loaded extensions
  end users can install without recompiling Wilfred: either a native
  `.dll`/`.so`/`.dylib` implementing the small C ABI in `plugin/abi.hpp`, or
  an external executable spoken to over a one-shot JSON request/response on
  stdio. See [plugins.md](plugins.md) for the manifest format and both
  protocols in full.

## IPC and the local API

Two local-only network surfaces exist, both used to control an already
running daemon:

* `ipc::IpcServer` — a lightweight local socket/named-pipe protocol
  (`ipc/protocol.hpp`) used by the CLI (`wilfred search`, `wilfred status`,
  etc.) to talk to a running `wilfred daemon` without duplicating the index
  in a second process.
* `ipc::HttpApiServer` — an optional (`api.enabled`, default `false`)
  `127.0.0.1`-bound HTTP API guarded by a bearer token, for scripting or
  third-party integrations.

Full request/response shapes are in [ipc-and-api.md](ipc-and-api.md).

## UI layer

The overlay is a native, borderless, always-on-top window per platform
(created by `create_overlay()`, `service/service.hpp`) hosting an embedded
web view that loads `ui/overlay/index.html` / `app.js` / `style.css`. C++
and the web UI talk to each other through:

* `overlay_bind(OverlayQuery, OverlaySubmit)` — registers the C++ callbacks
  the web UI invokes for "user typed a query" and "user selected a result
  with this action".
* `overlay_results_json()` — serializes a `vector<SearchResult>` to the JSON
  shape `app.js` expects.
* `overlay_pump()` — pumps the native event loop; called from the daemon's
  main loop.

`ui/icon.hpp` renders a small file-type icon as a data URL for the overlay
to embed directly without a filesystem round trip per result.

## Platform layer

`platform/native.hpp` and `platform/platform.hpp` declare the OS-specific
primitives every other module is allowed to depend on (paths, process
launch, window creation hooks, `platform_init()`). Implementations live
under `src/platform/`, split by OS. Modules like `fs/watcher.hpp`,
`hotkey/hotkey.hpp`, `ipc/server.hpp`, and `plugin/host.cpp` also contain
their own `#ifdef _WIN32` / `#ifdef __APPLE__` / else-Linux branches directly
(rather than going through `platform/`) where the OS API surface is small
and specific to that module — see each header's `.cpp` for the exact split.

## Cross-cutting concerns

* **Config** (`config/config.hpp`, `config/yaml.hpp`) is loaded once at
  startup into a single `Config` struct passed by `const&` almost
  everywhere; there is no live-reload — changes require restarting the
  daemon (`wilfred daemon`) or re-running the CLI.
* **History** (`history/history.hpp`) is the only subsystem with
  meaningful cross-request mutable state used as a ranking input; it's
  guarded by its own mutex so the search and IPC threads can both touch it.
* **Logging** (`core/log.hpp`) is a simple leveled logger writing to
  stderr and/or a file, level and path controlled by `logging.*` config.
* **Backup/Sync** (`sync/backup.hpp`) packs config + snippets + (optionally)
  the index into a single archive for local backup/restore or push/pull to
  a remote URL — see [sync-and-backup.md](sync-and-backup.md).
