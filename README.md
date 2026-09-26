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
* Intelligent content indexing for source, config, and text documents
* Mini results for weather, time, disk, RAM, CPU, processes, and more
* Search macros (`!yt`, `gh`, `wiki`, custom templates in config)
* Composable filters (`\*.cpp in Projects`, `type:image`, `size:>10mb`, named scopes)
* Application discovery (Start Menu / `.app` bundles / `.desktop` files)
* Default-browser detection, URL open, and web-search fallback
* Safe expression parser (arithmetic, functions, unit conversion) — no eval/code
* YAML configuration with validation and human-readable errors
* Global hotkey: `Ctrl+Alt+W` (Windows/Linux), `⌘+Alt+W` (macOS), configurable
* Local search history (optional, disable or clear)
* Overlay UI plus CLI (`search`, `launch`, `index`, `status`)



<a href="https://www.star-history.com/?repos=ajakfial%2Fwilfred\&type=timeline\&logscale=\&releases=\&legend=bottom-right">

&#x20;<picture>

&#x20;  <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/chart?repos=ajakfial/wilfred\&type=timeline\&theme=dark\&logscale\&legend=bottom-right" />

&#x20;  <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/chart?repos=ajakfial/wilfred\&type=timeline\&logscale\&legend=bottom-right" />

&#x20;  <img alt="Star History Chart" src="https://api.star-history.com/chart?repos=ajakfial/wilfred\&type=timeline\&logscale\&legend=bottom-right" />

&#x20;</picture>

</a>



## Build

Requires CMake 3.16+ and a C++20 compiler (MSVC, Apple Clang, or GCC).

```bash
cmake -S . -B build -DCMAKE\_BUILD\_TYPE=Release
cmake --build build --config Release
```

On Linux, X11 development headers enable the overlay (`libx11-dev`).

## Run

```bash
# Background daemon: overlay, indexer, hotkey
wilfred

# One-shot search against the local index
wilfred search firefox

# Index configured roots now
wilfred index

# Statistics
wilfred status
```

User config is created on first run:

|Platform|Config|Index data|
|-|-|-|
|Windows|`%APPDATA%\\Wilfred\\wilfred.yml`|`%LOCALAPPDATA%\\Wilfred\\`|
|macOS|`\~/Library/Application Support/Wilfred/`|same|
|Linux|`\~/.config/wilfred/wilfred.yml`|`\~/.local/share/wilfred/`|

See `config/wilfred.default.yml` for the full schema (index roots, excludes,
system directories, ranking weights, aliases, hotkey, history, browser).

## Query language (examples)

|Query|Meaning|
|-|-|
|`firefox`|Fuzzy file/app search|
|`25 \* 42`|Calculator|
|`https://example.com`|Open URL|
|`? cats` / `g cats`|Web search in the default browser|
|`\*.cpp in Projects`|Extension + directory filter|
|`type:image name:logo`|Kind + filename|
|`content:widget` / `intext foo`|Search indexed file contents|
|`> notepad`|Command / launch style|
|`scope:home notes`|Named directory group from config|
|`weather` / `weather London`|Mini card: local or city forecast|
|`time` `disk` `disku` `ram` `cpu`|Clock, drives, memory, processor|
|`process chrome` / `top`|Live process CPU, RAM, threads|
|`clip` / `clips`|Clipboard and recent clips|
|`!yt cats` / `gh wilfred`|Search macros (`macros` lists them)|

## Architecture

Platform I/O lives behind `wilfred/platform/native.hpp`. Core modules:

* `config` — YAML parse + schema validation
* `index` — string intern pool, compact records, inverted/trigram indexes, WAL
* `fs` — walker, classification, volumes, watcher
* `search` — fuzzy, ranking weights, filters, query cache
* `query` — classification + interpreter
* `math` — recursive-descent calculator
* `apps` / `browser` / `history` / `hotkey` / `ipc` / `ui` / `service`
* `providers` — registry for additional search backends

## Tests and benchmarks

```bash
cmake --build build --config Release --target wilfred\_tests wilfred\_bench
./build/wilfred\_tests          # or build/Release/wilfred\_tests.exe
./build/wilfred\_bench
```

Tests cover index CRUD, persistence/recovery, fuzzy/rank/filter, math, query
classification, config validation, history, paths, IPC, and application discovery.
Benchmarks measure intern, insert, query, fuzzy, ranking, filter, math, and snapshot I/O
on 1K–100K synthetic records.

