# (QUESTO README È STATO CREATO DA CURSOR AI, TUTTO IL RESTO, INCLUSO IL CODICE, È STATO SCRITTO A MANO DA PERSONA UMANA.)

# Wilfred

**Sito web:** https://ajakfial.github.io/Wilfred/

Wilfred è un motore di ricerca e launcher per applicazioni desktop veloce, leggero e multipiattaforma. È progettato come un livello di ricerca profondo del SO: file, cartelle, applicazioni, integrazione browser, calcolatrice e query filtrate da un'unica barra di ricerca richiamabile.

**VELOCE. PROFONDO. MODULARE. BASSO IMPATTO. MULTIPIATTAFORMA. ESTENSIBILE.**

## Funzionalità

* Indice incrementale, persistente di nomi file/percorsi/metadati con write-ahead log
* Osservatori filesystem (ReadDirectoryChangesW, FSEvents, inotify) più fallback di riescan
* Matching fuzzy, acronimi, ranking token/percorso, segnali recency e frequency
* Sicurezza typo: Damerau (transposition-aware) near-miss matching su nomi, token e percorsi, più correzione `Hai inteso` (comandi, storico, nomi file)
* Autocompletamento inline: ghost completion (`→` per accettare) + top-N candidati da history, comandi e indice
* Ranking context-aware (cartelle recenti, tipi file, ora del giorno, query sessione)
* Clipboard come fonte secondaria (text match, percorsi copiati, clip history)
* Clipboard manager persistente (`clips [query]`, pin, `clips clear`, `clips url|email|path|code|ip`)
* Content indexing per sorgenti, config e documenti testo
* Estrazione testo reale documenti (PDF, Office, RTF, HTML) nell'indice contenuti
* Ricerca semantica opzionale: embeddings locali (server/modello llama.cpp o hash built-in) + indice vettoriale HNSW (`vectors.bin` accanto a WAL/snapshot) fuso con trigram soft-match (`providers.semantic`, `embedding.*`)
* Assistente AI opzionale (`ai ...`/`ask ...`, screen-aware `ai see ...`) con tue chiavi: OpenAI, Anthropic, Gemini, Groq
* Snippets con cartelle, placeholder `{date}`/`{clipboard}`/`{query}`, e abbreviazioni globali opzionali in qualsiasi app
* Quick notes + todos (`note ...`, `todo ...`, persistenti nella data dir)
* Pomodoro / countdown timers + stopwatch (`timer`, `pomodoro`, `stopwatch`)
* Rich file previews (documenti, CSV, immagini, cartelle) nell'overlay (`F3`) più `wilfred preview`
* Calendario (`.ics`), contatti (`.vcf`), e note (Markdown) sorgenti + OCR opzionale via `tesseract` (`sources.*`)
* Cross-platform overlay (Windows WebView2 topmost, macOS floating panel, Linux WebKitGTK con fallback automatico X11 canvas, layer-shell ancorato su compositor Wayland, tutti above/skip-taskbar) con `Ctrl`/`⌘`+`K` actions popover e shortcut corrispondenti in entrambi i backend Linux
* Browser bookmarks, history, e open tabs (`bm`, provider-backed)
* Instant NTFS full-disk enumeration via USN journal su Windows
* OS index federation ufficiale (`os_search:`): Windows Search, Spotlight, Tracker/Baloo, `locate` riempiono gap che l'indice Wilfred manca (`os <query>` forza, duplicati rimossi)
* Preview pane nell'overlay (`F3`) più `wilfred preview`, e chained result actions (`open+copy_path`) + named multi-step workflows (`workflows:`)
* Mini results per weather, time, disk, RAM, CPU, processes, battery, windows, timers, notes, media, network, layouts, e altro
* Window management (`windows`, `minimize`/`maximize`/`close window`, snap) più saved window layouts (`layout save`, `layout`)
* Screen-aware AI (`ai see ...` invia screenshot come vision input al tuo provider)
* Quick notes ora vivono come veri file Markdown, ricercabili sia come carte che tramite notes search
* System-wide automation: extra global hotkeys (`hotkeys:`), `wilfred workflow` / `wilfred exec` CLI triggers
* Search macros (`!yt`, `gh`, `wiki`, plus custom templates in config) + parameterized quicklinks (`quicklinks:` con `{1}`/`{*}`/`{query}`)
* Per-app extra context actions (`app_actions:`) sopra le built-in file actions
* Large-file finder + duplicate candidates dall'indice (`large`, `dupes`)
* File conversion (`convert song.wav to mp3`, `convert photo.bmp to png`: native WAV/RAW + BMP/PNG/PPM/TGA, ffmpeg per mp3/ogg/flac/jpg/webp) + background removal (`bgremove photo.png` a PNG trasparente, nessun modello necessario)
* Process killer (`kill <pid|name>`), native media keys (`media play/next/mute/vol` -- no helpers to install), network tools (`ping`, `dns`, `myip`), speech-to-text (`transcribe` via whisper CLI o built-in spoke engine)
* Composable filters (`*.cpp in Projects`, `type:image`, `size:>10mb`, named scopes)
* Application discovery (Start Menu / `.app` bundles / `.desktop` files)
* Rich file actions (copy path flavors, SHA-256 hash, zip, terminal/editor here, new file/folder, Open With)
* Default-browser detection, URL open, and web-search fallback
* Safe expression parser (arithmetic, functions, unit/currency/color/date conversion, uuid/base64/sha256/lorem/json, hex/dec/bin/oct, bits, regex/url/jwt)
* Native and stdio plugins, plus extra search backends via providers
* Calculator plus unit, currency, color, date, and small dev utilities
* System commands from overlay (`lock`, `sleep`, `shutdown`, `restart`, `logout`, empty trash)
* System toggles (`wifi on/off`, `bluetooth toggle`, `volume 50/mute`, `brightness 70`) + settings deep-links (`settings wifi/bluetooth/sound/display/...`) -- real OS backends on Windows/macOS/Linux, honest unsupported errors on mobile/BSD gaps
* Package-manager search (`winget firefox`, `brew switch`, `apt vlc` -- Enter installs, `Tab` copies the command; `packages` lists detected managers)
* Settings + setup (`settings config open/validate`, `settings edit <key> <value>`, `setup`, `wilfred setup --overwrite`, `wilfred config-get/set/reset`, `wilfred config-validate/open/path`) and opt-in remote search backends (`remotes:` -- your endpoints only, off by default; per-source `headers:` + `max_results:`)
* Plugins with trust-on-first-use (`plugins` approval cards, `wilfred plugin list/install/approve/revoke`, optional `plugins.registry` index, sha256-verified installs)
* Window tiling (`layout tile halves|thirds|grid|columns N|rows N|stack`, `wilfred tile …`) + monitor-change auto-apply (`layouts.auto_apply`, `auto_layout`, `monitor_layouts`)
* Android app (Kotlin UI + C++ core via NDK → `.apk`): floating W button, search bar, same index/search/ranking -- see `docs/android.md`
* iOS app (SwiftUI + same C++ core → simulator `.app`): same search UI as Android (app-icon entry; iOS forbids overlay buttons), sandbox-only index -- see `docs/ios.md`
* Local backup/restore and optional remote sync (`backup`, `restore`, `sync-push`, `sync-pull`)
* Import from other launchers (`wilfred import --list/--detect/auto --dry-run/--overwrite`): Alfred, Raycast, PowerToys Run, Flow Launcher/Wox, Keypirinha, Listary, Ulauncher, Albert, KRunner, Rofi -- see `docs/import.md`
* YAML configuration with validation, `Did you mean` hints, and human-readable errors
* Global hotkey: `Ctrl+Alt+W` (Windows/Linux), `⌘+Alt+W` (macOS), configurable
* Local search history (optional; disable or `wilfred history-clear`)
* Overlay UI plus CLI (`search`, `launch`, `index`, `status`, backup/sync, `import`)

## Build

Richiede CMake 3.16+ e un compilatore C++20 (MSVC, Apple Clang, o GCC).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Su Linux, gli header di sviluppo X11 abilitano l'overlay (`libx11-dev`); su sessioni Wayland l'overlay WebKit diventa inoltre una superficie layer-shell nativa quando `libgtk-layer-shell-dev` è installato (Sway/wlroots, KDE Plasma -- GNOME non ha il protocollo layer-shell, dove la finestra rimane un semplice toplevel).

## Run

```bash
# Background daemon: overlay, indexer, hotkey
wilfred

# One-shot search against local index
wilfred search firefox

# Search and open top result
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

See `config/wilfred.default.yml` for the full schema (index roots, excludes, system directories, ranking weights, aliases, hotkey, history, browser).

## Migrating from another launcher

`wilfred import` brings hotkeys, custom web searches (as `macros:` + `quicklinks:`), snippets, aliases, theme, and the default search template into `wilfred.yml`. Merging is default; `--overwrite` replaces conflicts, `--dry-run` previews, and a `.pre-import.bak` backup is written before every real import.

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
| `? gatti` / `g gatti` | Web search in default browser |
| `*.cpp in Projects` | Extension + directory filter |
| `type:image name:logo` | Kind + filename |
| `content:widget` / `intext foo` | Search indexed file contents |
| `> notepad` | Command / launch style |
| `scope:home note` | Named directory group from config |
| `meteo` / `meteo Roma` | Mini card: local or city forecast |
| `speedtest` | Live ping, download, and upload (`speedtest again` reruns) |
| `ora` `disco` `discou` `ram` `cpu` | Clock, drives, memory, processor |
| `processo chrome` / `top` | Live process CPU, RAM, threads (`kill <pid|name>` terminates) |
| `screenshot` / `screenshot finestra` / `screenshot regione` | Capture fullscreen, window, or region to `Pictures/Wilfred` |
| `emoji sorriso` / `simbolo euro` | Emoji and symbol picker (enter copies) |
| `100 usd a eur` / `fx 25 gbp jpy` | Currency conversion |
| `1 cucchiaio a g` / `1 tazza farina a g` | Cooking volume↔mass (water default, optional ingredient) |
| `#ff5500` / `rgb(255, 85, 0)` / `colore coral` | Color conversion (hex / rgb / hsl) |
| `15pm est a pst` / `now in tokyo` / `tz londra` | Timezones |
| `oggi + 7 giorni` / `2024-01-31 + 1 mese` | Date math |
| `uuid` / `base64 ciao` / `sha256 abc` / `lorem 12` / `json {"a":1}` | Dev utilities (enter copies) |
| `hex 255` / `dec 0xff` / `0b1010` / `bit and 12 10` | Number bases + bit tools |
| `regex foo.* foobar` / `urlencode a b` / `jwt <token>` | Regex tester, URL codec, JWT decode |
| `timer 10m` / `pomodoro` / `cronometro start` | Countdowns, Pomodoro presets, stopwatch |
| `note compra latte` / `notes` / `todo spedisci` / `todos` | Quick notes + todos (`todo done 1`) |
| `media play` / `media next` / `media mute` | Native media keys + volume (Win media keys; mac HID + Music/Spotify/VLC; Linux native MPRIS D-Bus + wpctl/pactl/amixer, playerctl optional) |
| `ping example.com` / `dns example.com` / `mioip` | Ping, DNS lookup, public IP |
| `large 10` / `dupes` | Biggest files + duplicate candidates from index |
| `transcribe parla.mp3` / `ditta 10` | Speech-to-text for mp3/mp4 audio, mic dictation (`transcription.engine`: whisper CLI or built-in spoke engine) |
| `convert song.wav to mp3` / `convert photo.bmp to png` | File conversion (native PCM + BMP/PNG/PPM/TGA, ffmpeg for compressed) |
| `bgremove foto.png` / `bgremove foto.png 40 #ffffff` | Background removal to transparent PNG (auto corners or explicit color) |
| `finestre chrome` / `minimizza spotify` | List, switch, minimize, maximize, close, snap windows |
| `layout save lavoro` / `layout lavoro` | Save and restore window layouts |
| `define resilient` / `thesaurus happy` | Offline definition + synonyms (enter copies) |
| `pins` / `pin firefox` / `unpin firefox` | Pinned favorites (boosted via `ranking.pinned`) |
| `event add Riunione team | domani 10am` | Create a calendar event (writes `.ics`) |
| `contact add Jane Doe jane@x.com 555-0100` | Create a contact (writes `.vcf`) |
| `rename ./foto foto-{n}.jpg` | Bulk rename with `{n}` `{name}` `{ext}` |
| `template python foo in ./dir` | New file from template (md/python/cpp/html/json/gitignore) |
| `clips image` / `clips path` | Clipboard history filtered by type (text/url/email/path/code/ip/image) |
| `workflow revisione` / `run revisione` | Named multi-step workflows (`workflows` lists) |
| `ql docs ciao` / `ticket:ABC-123` | Parameterized quicklinks (`{1}`/`{*}`/`{query}`) |
| `lock` / `sleep` / `shutdown` / `restart` / `logout` / `empty trash` | System commands |
| `wifi` / `wifi off` / `bluetooth toggle` | Wi-Fi + Bluetooth status and on/off (enter applies) |
| `volume 50` / `volume mute` / `luminosita 70` | Volume level/mute + display brightness (enter applies) |
| `settings wifi` / `settings sound` / `settings` | Open OS settings pages (wifi/network/bluetooth/sound/display/...) |
| `setup` / `config validate` / `config open` | First-run status + edit/validate wilfred.yml (`wilfred setup`, `config-validate`) |
| `update` / `upgrade` | New-version status from startup check (enter copies link, `wilfred update` installs) |
| `settings edit search.max_results 40` / `settings get hotkey.key` | Update/read one setting in place, validated (`wilfred config-set/get`) |
| `plugins` / `plugins approve demo` | Plugin list + trust approvals (`wilfred plugin list/install/approve`) |
| `layout tile grid` / `tile halves` / `layout lavoro` | Tile open windows or apply saved layout (`wilfred tile`, `layouts.auto_apply` on monitor change) |
| `winget firefox` / `brew switch` / `apt vlc` | Package search -- Enter installs, `Tab` copies the command (`packages` lists managers) |
| `clip` / `clips` | Clipboard and recent clips |
| `!yt gatti` / `gh wilfred` | Search macros (`macros` lists them) |

## Architecture

Platform I/O lives behind `wilfred/platform/native.hpp`. Core modules:

* `config` -- YAML parse + schema validation
* `index` -- string intern pool, compact records, inverted/trigram indexes, WAL
* `fs` -- walker, classification, volumes, watcher
* `search` -- fuzzy, ranking weights, filters, query cache
* `query` -- classification + interpreter
* `math` -- recursive-descent calculator
* `apps` / `browser` / `history` / `hotkey` / `ipc` / `ui` / `service`
* `plugin` -- native `.dll`/`.so`/`.dylib` ABI and stdio plugins, trust store, registry index
* `sync` -- local backup archives and optional remote push/pull
* `import` -- cross-OS launcher settings importer (hotkeys, searches, snippets)
* `providers` -- registry for additional search backends (semantic, browser library, calendar/contacts/notes, remotes, packages)

## Tests and benchmarks

```bash
cmake --build build --config Release --target wilfred_tests wilfred_bench
./build/wilfred_tests          # or build/Release/wilfred_tests.exe
./build/wilfred_bench
```

Tests cover index CRUD, persistence/recovery, fuzzy/rank/filter, math, query classification, config validation, history, paths, IPC, and application discovery.

## Performance

Wilfred is a single C++20 binary with no vendored third-party libraries -- everything (JSON, YAML, mmap wrapper, CRC32) is implemented in-tree, so there is nothing extra to install (the Linux overlay uses system X11/WebKit libraries when present, plus gtk-layer-shell for native Wayland anchoring, with an X11-canvas fallback). The speed comes from the index design (see `docs/architecture.md`):

* Compact memory-mapped record store with a string intern pool (no duplicate path/name strings in RAM)
* Trigram + token indexes so queries hit posting lists, not full scans
* Write-ahead log with periodic snapshots and crash recovery -- writes are appends, reads never block on persistence
* Incremental, debounced filesystem-watcher updates instead of rescans
* Bounded result sets (top-40) with per-query timeouts on plugins

`wilfred_bench` (`benches/bench_main.cpp`) measures the hot paths on synthetic 1K-100K filename datasets (10 timed iterations + 1 warmup, 5 for the 100K insert) and reports Min / Median / Mean / P95 / Max milliseconds:

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

Run it on your own hardware before quoting numbers -- results depend on CPU, disk, and dataset shape:

```bash
cmake --build build --config Release --target wilfred_bench
./build/wilfred_bench          # or build/Release/wilfred_bench.exe
```

## Packaging

Pushing a `v*.*.*` tag runs `.github/workflows/release.yml`, which builds Release binaries and attaches one archive per platform to the GitHub Release (see `docs/building.md#packaging-what-ci-does`):

| Job | Artifacts |
|---|---|
| `windows-x64` | Portable `.zip`, signed `.msi` (WiX v3, per-user, no UAC), Chocolatey `.nupkg` (embedded portable, no downloads at install) |
| `linux-x64` | Portable `.tar.gz` |
| `freebsd-x64` | Portable `.tar.gz` (FreeBSD release job; other BSDs build from source -- see `docs/bsd.md`) |
| `macos-arm64` | Portable `.tar.gz` |
| `android` | `.apk` + `.aab` -- signed when keystore secrets exist, otherwise `-unsigned` suffixed (see `docs/android.md`) |
| `ios` | Unsigned simulator `.zip` (see `docs/ios.md`) |

Desktop archives stage the binary with `ui/overlay/`, `README.md`, and `config/wilfred.default.yml`. Windows binaries always carry the publisher stamp `Wilfred Open Contributors` via VERSIONINFO; Authenticode trust comes from PFX or Azure Trusted Signing secrets when configured (see `docs/signing.md`, `docs/installer.md`).

## Production deployment

* **Run the daemon, not one-shots.** `wilfred daemon` (bare `wilfred`) is the long-lived process: indexer + watcher + hotkey + overlay + IPC server. Run it as a login item / autostart entry (`wilfred autostart enable` registers this binary on Windows/macOS/Linux/BSD); the tray icon (Windows) quits it. CLI subcommands (`search`, `launch`, `status`, …) talk to the running daemon over the local IPC socket when available (see `docs/cli.md`, `docs/ipc-and-api.md`).
* **Config is code.** First run writes a validated `wilfred.yml` (locations table above); invalid keys fail with human-readable errors and `Did you mean` hints, never silent defaults. For fleets, distribute a baseline `wilfred.yml` (roots, excludes, weights, aliases -- full schema in `docs/configuration.md` and `config/wilfred.default.yml`) and let users override locally.
* **Data and DR.** Index, history, clips, notes, and logs live in the per-OS data dir (table above). `backup` / `restore` move a machine's state; `sync-push` / `sync-pull` replicate it (see `docs/sync-and-backup.md`). Test restores before you need them.
* **Observe it.** `wilfred status` reports index health; `logging.*` keys control log verbosity/retention; the optional local HTTP API (`api.*`, authenticated) exposes `/search`, `/status`, `/index`, `/launch`, and backup/sync endpoints for dashboards and automation.
* **Privacy posture.** Local-first: no account, no telemetry, no network calls unless you enable them (weather/AI/sync). Disable or cap history via `history.*` where policy requires; AI providers need the user's own API keys (`ai.*`, off by default).
* **Mobile is experimental (as of v23.0.5).** The Android (`.apk`) and iOS (simulator `.app`) builds install and run but are not daily-driver ready pending further testing and development -- deploy desktop first (see `docs/android.md`, `docs/ios.md`).