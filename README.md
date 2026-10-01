# (THIS README IS MADE BY CURSOR AI, EVERYTHING ELSE, INCLUDING CODE, IS WRITTEN BY HUMAN HAND.)

# Wilfred

Wilfred is a fast, lightweight, cross-platform desktop search engine and
application launcher. It is designed as a deep OS search layer: files, folders,
applications, browser integration, calculator, and filtered queries from a
single summonable search bar.

**FAST. DEEP. MODULAR. LOW-RESOURCE. CROSS-PLATFORM. EXTENSIBLE.**

## Features

* Incremental, persistent filename/path/metadata index with a write-ahead log
* Filesystem watchers (ReadDirectoryChangesW, FSEvents, inotify) plus rescan fallback
* Fuzzy matching, acronyms, token/path ranking, recency and frequency signals
* Context-aware ranking (recent folders, file types, time-of-day, session queries)
* Clipboard as a secondary source (text match, copied paths, clip history)
* Content indexing for source, config, and text documents
* Mini results for weather, time, disk, RAM, CPU, processes, battery, windows, and more
* Search macros (`!yt`, `gh`, `wiki`, plus custom templates in config)
* Composable filters (`*.cpp in Projects`, `type:image`, `size:>10mb`, named scopes)
* Application discovery (Start Menu / `.app` bundles / `.desktop` files)
* Default-browser detection, URL open, and web-search fallback
* Safe expression parser (arithmetic, functions, unit/currency/color/date conversion, uuid/base64/sha256/lorem/json)
* Native and stdio plugins, plus extra search backends via providers
* Calculator plus unit, currency, color, date, and small dev utilities
* System commands from the overlay (`lock`, `sleep`, `shutdown`, `restart`, `logout`, empty trash)
* Local backup/restore and optional remote sync (`backup`, `restore`, `sync-push`, `sync-pull`)
* YAML configuration with validation and human-readable errors
* Global hotkey: `Ctrl+Alt+W` (Windows/Linux), `⌘+Alt+W` (macOS), configurable
* Local search history (optional; disable or `wilfred history-clear`)
* Overlay UI plus CLI (`search`, `launch`, `index`, `status`, backup/sync)



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

On Linux, X11 development headers enable the overlay (`libx11-dev`).

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
```

User config is created on first run:

| Platform | Config | Index data |
|---|---|---|
| Windows | `%APPDATA%\Wilfred\wilfred.yml` | `%LOCALAPPDATA%\Wilfred\` |
| macOS | `~/Library/Application Support/Wilfred/` | same |
| Linux | `~/.config/wilfred/wilfred.yml` | `~/.local/share/wilfred/` |

See `config/wilfred.default.yml` for the full schema (index roots, excludes,
system directories, ranking weights, aliases, hotkey, history, browser).

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
| `process chrome` / `top` | Live process CPU, RAM, threads |
| `screenshot` / `screenshot window` / `screenshot region` | Capture fullscreen, window, or region to `Pictures/Wilfred` |
| `emoji smile` / `symbol euro` | Emoji and symbol picker (enter copies) |
| `100 usd to eur` / `fx 25 gbp jpy` | Currency conversion |
| `1 tbsp to g` / `1 cup flour to g` | Cooking volume↔mass (water default, optional ingredient) |
| `#ff5500` / `rgb(255, 85, 0)` / `color coral` | Color conversion (hex / rgb / hsl) |
| `3pm est to pst` / `now in tokyo` / `tz london` | Timezones |
| `today + 7 days` / `2024-01-31 + 1 month` | Date math |
| `uuid` / `base64 hi` / `sha256 abc` / `lorem 12` / `json {"a":1}` | Dev utilities (enter copies) |
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
* `providers` — registry for additional search backends

## Tests and benchmarks

```bash
cmake --build build --config Release --target wilfred_tests wilfred_bench
./build/wilfred_tests          # or build/Release/wilfred_tests.exe
./build/wilfred_bench
```

Tests cover index CRUD, persistence/recovery, fuzzy/rank/filter, math, query
classification, config validation, history, paths, IPC, and application discovery.
Benchmarks measure intern, insert, query, fuzzy, ranking, filter, math, and snapshot I/O
on 1K–100K synthetic records.

