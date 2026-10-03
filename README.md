# (THIS README IS MADE BY CURSOR AI, EVERYTHING ELSE, INCLUDING CODE, IS WRITTEN BY HUMAN HAND.)

# Wilfred

**Website:** https://ajakfial.github.io/Wilfred/

Wilfred is a fast, lightweight, cross-platform desktop search engine and
application launcher. It is designed as a deep OS search layer: files, folders,
applications, browser integration, calculator, and filtered queries from a
single summonable search bar.

**FAST. DEEP. MODULAR. LOW-RESOURCE. CROSS-PLATFORM. EXTENSIBLE.**

## Features

* Incremental, persistent filename/path/metadata index with a write-ahead log
* Filesystem watchers (ReadDirectoryChangesW, FSEvents, inotify) plus rescan fallback
* Fuzzy matching, acronyms, token/path ranking, recency and frequency signals
* Typo safety: Damerau (transposition-aware) near-miss matching on names, tokens and paths, plus `Did you mean` correction (commands, history, filenames)
* Inline autocomplete: ghost completion (`→` to accept) + top-N candidates from history, commands and the index
* Context-aware ranking (recent folders, file types, time-of-day, session queries)
* Clipboard as a secondary source (text match, copied paths, clip history)
* Persistent clipboard manager (`clips [query]`, pin, `clips clear`, `clips url|email|path|code|ip`)
* Content indexing for source, config, and text documents
* Real document text extraction (PDF, Office, RTF, HTML) into the content index
* Optional semantic search: local embeddings (llama.cpp server/model or built-in hash) + HNSW vector index (`vectors.bin` next to WAL/snapshot) merged with trigram soft-match (`providers.semantic`, `embedding.*`)
* Optional AI assistant (`ai ...`/`ask ...`, screen-aware `ai see ...`) with your own keys: OpenAI, Anthropic, Gemini, Groq
* Snippets with folders, `{date}`/`{clipboard}`/`{query}` placeholders, and optional global abbreviations in any app
* Quick notes + todos (`note ...`, `todo ...`, persistent in the data dir)
* Pomodoro / countdown timers + stopwatch (`timer`, `pomodoro`, `stopwatch`)
* Rich file previews (documents, CSV, images, folders) in the overlay (`F3`) plus `wilfred preview`
* Calendar (`.ics`), contacts (`.vcf`), and notes (Markdown) sources + optional OCR indexing via `tesseract` (`sources.*`)
* Cross-platform overlay (Windows WebView2 topmost, macOS floating panel, Linux WebKitGTK with automatic X11 canvas fallback, layer-shell anchored on Wayland compositors, all above/skip-taskbar) with `Ctrl`/`⌘`+`K` actions popover and matching shortcuts in both Linux backends
* Browser bookmarks, history, and open tabs (`bm`, provider-backed)
* Instant NTFS full-disk enumeration via the USN journal on Windows
* Preview pane in the overlay (`F3`) plus `wilfred preview`, and chained result actions (`open+copy_path`) + named multi-step workflows (`workflows:`)
* Mini results for weather, time, disk, RAM, CPU, processes, battery, windows, timers, notes, media, network, layouts, and more
* Window management (`windows`, `minimize`/`maximize`/`close window`, snap) plus saved window layouts (`layout save`, `layout`)
* Screen-aware AI (`ai see ...` sends a screenshot as vision input to your provider)
* Quick notes now live as real Markdown files, searchable both as cards and through notes search
* System-wide automation: extra global hotkeys (`hotkeys:`), `wilfred workflow` / `wilfred exec` CLI triggers
* Search macros (`!yt`, `gh`, `wiki`, plus custom templates in config) + parameterized quicklinks (`quicklinks:` with `{1}`/`{*}`/`{query}`)
* Per-app extra context actions (`app_actions:`) on top of the built-in file actions
* Large-file finder + duplicate candidates from the index (`large`, `dupes`)
* Process killer (`kill <pid|name>`), native media keys (`media play/next/mute/vol` — no helpers to install), network tools (`ping`, `dns`, `myip`), speech-to-text (`transcribe` via whisper CLI)
* Composable filters (`*.cpp in Projects`, `type:image`, `size:>10mb`, named scopes)
* Application discovery (Start Menu / `.app` bundles / `.desktop` files)
* Rich file actions (copy path flavors, SHA-256 hash, zip, terminal/editor here, new file/folder, Open With)
* Default-browser detection, URL open, and web-search fallback
* Safe expression parser (arithmetic, functions, unit/currency/color/date conversion, uuid/base64/sha256/lorem/json, hex/dec/bin/oct, bits, regex/url/jwt)
* Native and stdio plugins, plus extra search backends via providers
* Calculator plus unit, currency, color, date, and small dev utilities
* System commands from the overlay (`lock`, `sleep`, `shutdown`, `restart`, `logout`, empty trash)
* Android app (Kotlin UI + C++ core via NDK → `.apk`): floating W button, search bar, same index/search/ranking — see `docs/android.md`
* iOS app (SwiftUI + same C++ core → simulator `.app`): same search UI as Android (app-icon entry; iOS forbids overlay buttons), sandbox-only index — see `docs/ios.md`
* Local backup/restore and optional remote sync (`backup`, `restore`, `sync-push`, `sync-pull`)
* Import from other launchers (`wilfred import --list/--detect/auto --dry-run/--overwrite`): Alfred, Raycast, PowerToys Run, Flow Launcher/Wox, Keypirinha, Listary, Ulauncher, Albert, KRunner, Rofi — see `docs/import.md`
* YAML configuration with validation, `Did you mean` hints, and human-readable errors
* Global hotkey: `Ctrl+Alt+W` (Windows/Linux), `⌘+Alt+W` (macOS), configurable
* Local search history (optional; disable or `wilfred history-clear`)
* Overlay UI plus CLI (`search`, `launch`, `index`, `status`, backup/sync, `import`)



<a href="https://www.star-history.com/?repos=ajakfial%2Fwilfred&type=timeline&legend=bottom-right">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/chart?repos=ajakfial/wilfred&type=timeline&theme=dark&legend=bottom-right" />
    <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/chart?repos=ajakfial/wilfred&type=timeline&legend=bottom-right" />
    <img alt="Star History Chart" src="https://api.star-history.com/chart?repos=ajakfial/wilfred&type=timeline&legend=bottom-right" />
  </picture>
</a>



## Build

Requires CMake 3.16+ and a C++20 compiler (MSVC, Apple Clang, or GCC).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

On Linux, X11 development headers enable the overlay (`libx11-dev`); on
Wayland sessions the WebKit overlay additionally becomes a native
layer-shell surface when `libgtk-layer-shell-dev` is installed
(Sway/wlroots, KDE Plasma — GNOME has no layer-shell protocol, where the
window stays a plain toplevel).

## Run

```bash
# Background daemon: overlay, indexer, hotkey
wilfred

# One-shot search against the local index
wilfred search firefox

# Search and open the top result
wilfred launch firefox

# Index configured roots now
wilfred index

# Statistics
wilfred status

# Import settings from another launcher
wilfred import --list
wilfred import --detect
wilfred import auto --dry-run
wilfred import alfred --from Alfred.alfredpreferences --dry-run
```

User config is created on first run:

| Platform | Config | Index data |
|---|---|---|
| Windows | `%APPDATA%\Wilfred\wilfred.yml` | `%LOCALAPPDATA%\Wilfred\` |
| macOS | `~/Library/Application Support/Wilfred/` | same |
| Linux | `~/.config/wilfred/wilfred.yml` | `~/.local/share/wilfred/` |
| BSD | `~/.config/wilfred/wilfred.yml` | `~/.local/share/wilfred/` |

See `config/wilfred.default.yml` for the full schema (index roots, excludes,
system directories, ranking weights, aliases, hotkey, history, browser).

## Migrating from another launcher

`wilfred import` brings hotkeys, custom web searches (as `macros:` +
`quicklinks:`), snippets, aliases, theme, and the default search template
into `wilfred.yml`. Merging is the default; `--overwrite` replaces
conflicts, `--dry-run` previews, and a `.pre-import.bak` backup is written
before every real import.

| OS | Supported sources |
|---|---|
| macOS | Alfred (`Alfred.alfredpreferences` bundle), Raycast (quicklinks JSON) |
| Windows | PowerToys Run, Flow Launcher, Wox, Keypirinha, Listary |
| Linux | Ulauncher, Albert, KRunner / KDE, Rofi |

See `docs/import.md` (and `wilfred import --list`) for per-launcher mapping.

## Query language (examples)

| Query | Meaning |
|---|---|
| `firefox` | Fuzzy file/app search |
| `25 * 42` | Calculator |
| `https://example.com` | Open URL |
| `? cats` / `g cats` | Web search in the default browser |
| `*.cpp in Projects` | Extension + directory filter |
| `type:image name:logo` | Kind + filename |
| `content:widget` / `intext foo` | Search indexed file contents |
| `> notepad` | Command / launch style |
| `scope:home notes` | Named directory group from config |
| `weather` / `weather London` | Mini card: local or city forecast |
| `speedtest` | Live ping, download, and upload (`speedtest again` reruns) |
| `time` `disk` `disku` `ram` `cpu` | Clock, drives, memory, processor |
| `process chrome` / `top` | Live process CPU, RAM, threads (`kill <pid|name>` terminates) |
| `screenshot` / `screenshot window` / `screenshot region` | Capture fullscreen, window, or region to `Pictures/Wilfred` |
| `emoji smile` / `symbol euro` | Emoji and symbol picker (enter copies) |
| `100 usd to eur` / `fx 25 gbp jpy` | Currency conversion |
| `1 tbsp to g` / `1 cup flour to g` | Cooking volume↔mass (water default, optional ingredient) |
| `#ff5500` / `rgb(255, 85, 0)` / `color coral` | Color conversion (hex / rgb / hsl) |
| `3pm est to pst` / `now in tokyo` / `tz london` | Timezones |
| `today + 7 days` / `2024-01-31 + 1 month` | Date math |
| `uuid` / `base64 hi` / `sha256 abc` / `lorem 12` / `json {"a":1}` | Dev utilities (enter copies) |
| `hex 255` / `dec 0xff` / `0b1010` / `bit and 12 10` | Number bases + bit tools |
| `regex foo.* foobar` / `urlencode a b` / `jwt <token>` | Regex tester, URL codec, JWT decode |
| `timer 10m` / `pomodoro` / `stopwatch start` | Countdowns, Pomodoro presets, stopwatch |
| `note buy milk` / `notes` / `todo ship it` / `todos` | Quick notes + todos (`todo done 1`) |
| `media play` / `media next` / `media mute` | Native media keys + volume (Win media keys; mac HID + Music/Spotify/VLC; Linux native MPRIS D-Bus + wpctl/pactl/amixer, playerctl optional) |
| `ping example.com` / `dns example.com` / `myip` | Ping, DNS lookup, public IP |
| `large 10` / `dupes` | Biggest files + duplicate candidates from the index |
| `transcribe talk.mp3` / `dictate 10` | Speech-to-text for mp3/mp4 audio, mic dictation (needs whisper CLI) |
| `windows chrome` / `minimize spotify` | List, switch, minimize, maximize, close, snap windows |
| `layout save work` / `layout work` | Save and restore window layouts |
| `clips url` / `clips code` | Clipboard history filtered by type (url/email/path/code/ip) |
| `workflow review` / `run review` | Named multi-step workflows (`workflows` lists) |
| `ql docs hello` / `ticket:ABC-123` | Parameterized quicklinks (`{1}` `{*}` `{query}`) |
| `lock` / `sleep` / `shutdown` / `restart` / `logout` / `empty trash` | System commands |
| `clip` / `clips` | Clipboard and recent clips |
| `!yt cats` / `gh wilfred` | Search macros (`macros` lists them) |

## Architecture

Platform I/O lives behind `wilfred/platform/native.hpp`. Core modules:

* `config` — YAML parse + schema validation
* `index` — string intern pool, compact records, inverted/trigram indexes, WAL
* `fs` — walker, classification, volumes, watcher
* `search` — fuzzy, ranking weights, filters, query cache
* `query` — classification + interpreter
* `math` — recursive-descent calculator
* `apps` / `browser` / `history` / `hotkey` / `ipc` / `ui` / `service`
* `plugin` — native `.dll`/`.so`/`.dylib` ABI and stdio plugins
* `sync` — local backup archives and optional remote push/pull
* `import` — cross-OS launcher settings importer (hotkeys, searches, snippets)
* `providers` — registry for additional search backends

## Tests and benchmarks

```bash
cmake --build build --config Release --target wilfred_tests wilfred_bench
./build/wilfred_tests          # or build/Release/wilfred_tests.exe
./build/wilfred_bench
```

Tests cover index CRUD, persistence/recovery, fuzzy/rank/filter, math, query
classification, config validation, history, paths, IPC, and application discovery.

## Performance

Wilfred is a single C++20 binary with no vendored third-party libraries —
everything (JSON, YAML, mmap wrapper, CRC32) is implemented in-tree, so
there is nothing extra to install (the Linux overlay uses system X11/WebKit
libraries when present, plus gtk-layer-shell for native Wayland anchoring,
with an X11-canvas fallback). The
speed comes from the index design (see `docs/architecture.md`):

* Compact memory-mapped record store with a string intern pool (no duplicate
  path/name strings in RAM)
* Trigram + token indexes so queries hit posting lists, not full scans
* Write-ahead log with periodic snapshots and crash recovery — writes are
  appends, reads never block on persistence
* Incremental, debounced filesystem-watcher updates instead of rescans
* Bounded result sets (top-40) with per-query timeouts on plugins

`wilfred_bench` (`benches/bench_main.cpp`) measures the hot paths on
synthetic 1K–100K filename datasets (10 timed iterations + 1 warmup, 5 for
the 100K insert) and reports Min / Median / Mean / P95 / Max milliseconds:

| Benchmark | What it measures |
|---|---|
| String pool: intern 100K | Interning throughput (index memory efficiency) |
| Tokenizer: tokenize 100K | Filename tokenization |
| Index insert: 1K / 10K / 100K | Bulk index build |
| Index query: 10K / 100K records | Posting-list lookup |
| Fuzzy search: 10K targets | Damerau-aware scoring throughput |
| Ranking: 10K scored hits | Weight scoring + top-40 selection |
| Filter chain: 10K records | Extension/kind filter evaluation |
| Math evaluation: 1K expressions | Calculator parse + eval |
| Snapshot save / load: 10K | Persistence round-trip |
| Memory reporting: 100K index | Index footprint accounting |

Run it on your own hardware before quoting numbers — results depend on CPU,
disk, and dataset shape:

```bash
cmake --build build --config Release --target wilfred_bench
./build/wilfred_bench          # or build/Release/wilfred_bench.exe
```

## Packaging

Pushing a `v*.*.*` tag runs `.github/workflows/release.yml`, which builds
Release binaries and attaches one archive per platform to the GitHub Release
(see `docs/building.md#packaging-what-ci-does`):

| Job | Artifacts |
|---|---|
| `windows-x64` | Portable `.zip`, signed `.msi` (WiX v3, per-user, no UAC), Chocolatey `.nupkg` (embedded portable, no downloads at install) |
| `linux-x64` | Portable `.tar.gz` |
| `freebsd-x64` | Portable `.tar.gz` (FreeBSD release job; other BSDs build from source — see `docs/bsd.md`) |
| `macos-arm64` | Portable `.tar.gz` |
| `android` | `.apk` + `.aab` — signed when keystore secrets exist, otherwise `-unsigned` suffixed (see `docs/android.md`) |
| `ios` | Unsigned simulator `.zip` (see `docs/ios.md`) |

Desktop archives stage the binary with `ui/overlay/`, `README.md`, and
`config/wilfred.default.yml`. Windows binaries always carry the publisher
stamp `Wilfred Open Contributors` via VERSIONINFO; Authenticode trust comes
from PFX or Azure Trusted Signing secrets when configured
(see `docs/signing.md`, `docs/installer.md`).

## Production deployment

* **Run the daemon, not one-shots.** `wilfred daemon` (bare `wilfred`) is the
  long-lived process: indexer + watcher + hotkey + overlay + IPC server.
  Run it as a login item / autostart entry; the tray icon (Windows) quits it.
  CLI subcommands (`search`, `launch`, `status`, …) talk to the running
  daemon over the local IPC socket when available (see `docs/cli.md`,
  `docs/ipc-and-api.md`).
* **Config is code.** First run writes a validated `wilfred.yml`
  (locations table above); invalid keys fail with human-readable errors and
  `Did you mean` hints, never silent defaults. For fleets, distribute a
  baseline `wilfred.yml` (roots, excludes, weights, aliases — full schema in
  `docs/configuration.md` and `config/wilfred.default.yml`) and let users
  override locally.
* **Data and DR.** Index, history, clips, notes, and logs live in the per-OS
  data dir (table above). `backup` / `restore` move a machine's state;
  `sync-push` / `sync-pull` replicate it (see `docs/sync-and-backup.md`).
  Test restores before you need them.
* **Observe it.** `wilfred status` reports index health; `logging.*` keys
  control log verbosity/retention; the optional local HTTP API (`api.*`,
  authenticated) exposes `/search`, `/status`, `/index`, `/launch`, and
  backup/sync endpoints for dashboards and automation.
* **Privacy posture.** Local-first: no account, no telemetry, no network
  calls unless you enable them (weather/AI/sync). Disable or cap history
  via `history.*` where policy requires; AI providers need the user's own
  API keys (`ai.*`, off by default).
* **Mobile is experimental (as of v23.0.5).** The Android (`.apk`) and iOS
  (simulator `.app`) builds install and run but are not daily-driver ready
  pending further testing and development — deploy desktop first
  (see `docs/android.md`, `docs/ios.md`).

