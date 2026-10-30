# (DEZE README IS GEMAAKT DOOR CURSOR AI, ALLES ANDERS, INCLUSIEF CODE, IS GESCHREVEN door MENSCHELIJKE HAND.)

# Wilfred

**Website:** https://ajakfial.github.io/Wilfred/

Wilfred is een snelle, lichtgewicht, cross-platform desktop zoekmachine en applicatie launcher. Het is ontworpen als een diepe OS zoeklaag: bestanden, mappen, applicaties, browserintegratie, rekentool, en gefilterde queries vanuit een enkele oproepbare zoekbalk.

**FAST. DIEPEGGAAND. MODULAIR. LAAG RESOURCE. CROSS-PLATFORM. UITBREIDBAAR.**

## Functies

* Incrementieel, persistent bestandsnaam/pad/metadata index met write-ahead log
* Filesystem watchers (ReadDirectoryChangesW, FSEvents, inotify) plus rescan fallback
* Fuzzy matching, acroniemen, token/pad ranking, recentie en frequentie signalen
* Typo veiligheid: Damerau (transpositie-bewuste) near-miss matching op namen, tokens en paden, plus `Meende je` correctie (commando's, geschiedenis, bestandsnamen)
* Inline autocomplete: ghost completion (`→` om te accepteren) + top-N kandidaten uit geschiedenis, commando's en index
* Context-aware ranking (recente mappen, bestands types, tijd van de dag, sessie queries)
* Clipboard als secundaire bron (tekst match, gekopieerde paden, clip history)
* Persistent clipboard manager (`clips [query]`, pin, `clips clear`, `clips url|email|path|code|ip`)
* Content indexing voor bron, config, en tekst documenten
* Echte document tekst extractie (PDF, Office, RTF, HTML) in de content index
* Optionele semantic search: lokale embeddings (llama.cpp server/model of built-in hash) + HNSW vector index (`vectors.bin` naast WAL/snapshot) samengevoegd met trigram soft-match (`providers.semantic`, `embedding.*`)
* Optionele AI assistant (`ai ...`/`ask ...`, scherm-bewuste `ai see ...`) met je eigen keys: OpenAI, Anthropic, Gemini, Groq
* Snippets met mappen, `{date}`/`{clipboard}`/`{query}` placeholders, en optionele globale afkortingen in elke app
* Quick notes + todos (`note ...`, `todo ...`, persistent in de data dir)
* Pomodoro / countdown timers + stopwatch (`timer`, `pomodoro`, `stopwatch`)
* Rijke file previews (documenten, CSV, afbeeldingen, mappen) in de overlay (`F3`) plus `wilfred preview`
* Kalender (`.ics`), contacten (`.vcf`), en notities (Markdown) bronnen + optionele OCR indexing via `tesseract` (`sources.*`)
* Cross-platform overlay (Windows WebView2 topmost, macOS zwevend panel, Linux WebKitGTK met automatische X11 canvas fallback, layer-shell geankerd op Wayland compositors, alle above/skip-taskbar) met `Ctrl`/`⌘`+`K` acties popover en overeenkomende shortcuts in beide Linux backends
* Browser bookmarks, history, en open tabs (`bm`, provider-backed)
* Directe NTFS full-disk enumeratie via USN journal op Windows
* Officiële OS index federatie (`os_search:`): Windows Search, Spotlight, Tracker/Baloo, `locate` vullen gaten die Wilfred's index mist (`os <query>` dwingt het, duplicaten gedropt)
* Preview pane in de overlay (`F3`) plus `wilfred preview`, en gekoppelde result acties (`open+copy_path`) + genaamde multi-stap workflows (`workflows:`)
* Mini results voor weer, tijd, schijf, RAM, CPU, processen, batterij, vensters, timers, notities, media, netwerk, layouts, en meer
* Window management (`windows`, `minimize`/`maximize`/`close window`, snap) plus opgeslagen window layouts (`layout save`, `layout`)
* Screen-aware AI (`ai see ...` stuurt screenshot als vision input naar je provider)
* Quick notes leven nu als echte Markdown bestanden, doorzoekbaar zowel als kaarten als via notes search
* System-wide automation: extra global hotkeys (`hotkeys:`), `wilfred workflow` / `wilfred exec` CLI triggers
* Zoek macro's (`!yt`, `gh`, `wiki`, plus custom templates in config) + parameterized quicklinks (`quicklinks:` met `{1}`/`{*}`/`{query}`)
* Per-app extra context actions (`app_actions:`) bovenop de built-in file actions
* Grote-bestand vinders + duplicaat kandidaten uit de index (`large`, `dupes`)
* File conversie (`convert song.wav to mp3`, `convert photo.bmp to png`: native WAV/RAW + BMP/PNG/PPM/TGA, ffmpeg voor mp3/ogg/flac/jpg/webp) + achtergrond verwijdering (`bgremove photo.png` naar transparante PNG, geen model nodig)
* Proces killer (`kill <pid|name>`), native media keys (`media play/next/mute/vol` -- geen helpers te installeren), netwerk tools (`ping`, `dns`, `myip`), speech-to-text (`transcribe` via whisper CLI of built-in spoke engine)
* Composeerbare filters (`*.cpp in Projects`, `type:image`, `size:>10mb`, named scopes)
* Applicatie ontdekking (Start Menu / `.app` bundles / `.desktop` files)
* Rijke file acties (copy path flavors, SHA-256 hash, zip, terminal/editor hier, nieuw bestand/map, Open With)
* Default-browser detectie, URL open, en web-search fallback
* Veilige expressie parser (arithmetic, functies, unit/currency/color/date conversie, uuid/base64/sha256/lorem/json, hex/dec/bin/oct, bits, regex/url/jwt)
* Native en stdio plugins, plus extra search backends via providers
* Calculator plus unit, currency, color, date, en kleine dev utilities
* System commands from overlay (`lock`, `sleep`, `shutdown`, `restart`, `logout`, empty trash)
* System toggles (`wifi on/off`, `bluetooth toggle`, `volume 50/mute`, `brightness 70`) + settings deep-links (`settings wifi/bluetooth/sound/display/...`) -- echte OS backends op Windows/macOS/Linux, eerlijke unsupported errors op mobile/BSD gaps
* Package-manager search (`winget firefox`, `brew switch`, `apt vlc` -- Enter installeert, `Tab` kopieert commando; `packages` lijst gedetecteerde managers)
* Settings + setup (`settings config open/validate`, `settings edit <key> <value>`, `setup`, `wilfred setup --overwrite`, `wilfred config-get/set/reset`, `wilfred config-validate/open/path`) en opt-in remote search backends (`remotes:` -- jouw endpoints only, off by default; per-source `headers:` + `max_results:`)
* Plugins met trust-on-first-use (`plugins` approval cards, `wilfred plugin list/install/approve/revoke`, optionele `plugins.registry` index, sha256-geverifieerde installs)
* Window tiling (`layout tile halves|thirds|grid|columns N|rows N|stack`, `wilfred tile …`) + monitor-change auto-apply (`layouts.auto_apply`, `auto_layout`, `monitor_layouts`)
* Android app (Kotlin UI + C++ core via NDK → `.apk`): zwevende W knop, zoekbalk, zelfde index/zoek/ranking -- zie `docs/android.md`
* iOS app (SwiftUI + same C++ core → simulator `.app`): dezelfde zoek UI als Android (app-icon entry; iOS verbiedt overlay buttons), sandbox-only index -- zie `docs/ios.md`
* Local backup/restore en optionele remote sync (`backup`, `restore`, `sync-push`, `sync-pull`)
* Import van andere launchers (`wilfred import --list/--detect/auto --dry-run/--overwrite`): Alfred, Raycast, PowerToys Run, Flow Launcher/Wox, Keypirinha, Listary, Ulauncher, Albert, KRunner, Rofi -- zie `docs/import.md`
* YAML configuratie met validatie, `Meende je` hints, en menselijk leesbare fouten
* Globale hotkey: `Ctrl+Alt+W` (Windows/Linux), `⌘+Alt+W` (macOS), configureerbaar
* Lokale zoekgeschiedenis (optioneel; uitschakelbaar of `wilfred history-clear`)
* Overlay UI plus CLI (`search`, `launch`, `index`, `status`, backup/sync, `import`)

## Build

Vereist CMake 3.16+ en een C++20 compiler (MSVC, Apple Clang, of GCC).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Op Linux, X11 development headers enable de overlay (`libx11-dev`); op Wayland sessies wordt de WebKit overlay additioneel een native layer-shell surface wanneer `libgtk-layer-shell-dev` geïnstalleerd is (Sway/wlroots, KDE Plasma -- GNOME heeft geen layer-shell protocol, waar het venster een plain toplevel blijft).

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

Gebruikersconfig wordt bij eerste run aangemaakt:

| Platform | Config | Index data |
|---|---|---|
| Windows | `%APPDATA%\Wilfred\wilfred.yml` | `%LOCALAPPDATA%\Wilfred\` |
| macOS | `~/Library/Application Support/Wilfred/` | same |
| Linux | `~/.config/wilfred/wilfred.yml` | `~/.local/share/wilfred/` |
| BSD | `~/.config/wilfred/wilfred.yml` | `~/.local/share/wilfred/` |

Zie `config/wilfred.default.yml` voor het volledige schema (index roots, excludes, system directories, ranking weights, aliases, hotkey, history, browser).

## Migrating from another launcher

`wilfred import` brengt hotkeys, custom web searches (als `macros:` + `quicklinks:`), snippets, aliases, thema, en de standaard zoektemplate in `wilfred.yml`. Merging is standaard; `--overwrite` vervangt conflicten, `--dry-run` preview, en een `.pre-import.bak` backup wordt geschreven vóór elke echte import.

| OS | Supported sources |
|---|---|
| macOS | Alfred (`Alfred.alfredpreferences` bundle), Raycast (quicklinks JSON) |
| Windows | PowerToys Run, Flow Launcher, Wox, Keypirinha, Listary |
| Linux | Ulauncher, Albert, KRunner / KDE, Rofi |

Zie `docs/import.md` (en `wilfred import --list`) voor per-launcher mapping.

## Query language (examples)

| Query | Meaning |
|---|---|
| `firefox` | Fuzzy file/app search |
| `25 * 42` | Calculator |
| `https://example.com` | Open URL |
| `? katten` / `g katten` | Web search in default browser |
| `*.cpp in Projects` | Extension + directory filter |
| `type:image name:logo` | Kind + filename |
| `content:widget` / `intext foo` | Search indexed file contents |
| `> notepad` | Command / launch style |
| `scope:home notities` | Named directory group from config |
| `weer` / `weer Amsterdam` | Mini card: local or city forecast |
| `speedtest` | Live ping, download, and upload (`speedtest again` reruns) |
| `tijd` `schijf` `schijfu` `ram` `cpu` | Klok, drives, memory, processor |
| `proces chrome` / `top` | Live process CPU, RAM, threads (`kill <pid|name>` terminates) |
| `screenshot` / `screenshot window` / `screenshot region` | Capture fullscreen, window, or region to `Pictures/Wilfred` |
| `emoji smile` / `symbool euro` | Emoji and symbol picker (enter copies) |
| `100 usd to eur` / `fx 25 gbp jpy` | Currency conversion |
| `1 eetlepel to g` / `1 kop bloem to g` | Cooking volume↔mass (water default, optional ingredient) |
| `#ff5500` / `rgb(255, 85, 0)` / `kleur coral` | Color conversion (hex / rgb / hsl) |
| `15u est to pst` / `now in tokyo` / `tz londen` | Timezones |
| `vandaag + 7 dagen` / `2024-01-31 + 1 maand` | Date math |
| `uuid` / `base64 hoi` / `sha256 abc` / `lorem 12` / `json {"a":1}` | Dev utilities (enter copies) |
| `hex 255` / `dec 0xff` / `0b1010` / `bit and 12 10` | Number bases + bit tools |
| `regex foo.* foobar` / `urlencode a b` / `jwt <token>` | Regex tester, URL codec, JWT decode |
| `timer 10m` / `pomodoro` / `stopwatch start` | Countdowns, Pomodoro presets, stopwatch |
| `note koop melk` / `notes` / `todo verzend` / `todos` | Quick notes + todos (`todo done 1`) |
| `media play` / `media next` / `media mute` | Native media keys + volume (Win media keys; mac HID + Music/Spotify/VLC; Linux native MPRIS D-Bus + wpctl/pactl/amixer, playerctl optional) |
| `ping example.com` / `dns example.com` / `myip` | Ping, DNS lookup, public IP |
| `large 10` / `dupes` | Biggest files + duplicate candidates from index |
| `transcribe praat.mp3` / `dicteer 10` | Speech-to-text for mp3/mp4 audio, mic dictation (`transcription.engine`: whisper CLI or built-in spoke engine) |
| `convert song.wav to mp3` / `convert photo.bmp to png` | File conversion (native PCM + BMP/PNG/PPM/TGA, ffmpeg for compressed) |
| `bgremove photo.png` / `bgremove photo.png 40 #ffffff` | Background removal to transparent PNG (auto corners or explicit color) |
| `vensters chrome` / `minimaliseer spotify` | List, switch, minimize, maximize, close, snap windows |
| `layout save werk` / `layout werk` | Save and restore window layouts |
| `define resilient` / `thesaurus happy` | Offline definition + synonyms (enter copies) |
| `pins` / `pin firefox` / `unpin firefox` | Pinned favorites (boosted via `ranking.pinned`) |
| `event add Team sync | morgen 10am` | Create a calendar event (writes `.ics`) |
| `contact add Jane Doe jane@x.com 555-0100` | Create a contact (writes `.vcf`) |
| `rename ./fotos foto-{n}.jpg` | Bulk rename with `{n}` `{name}` `{ext}` |
| `template python foo in ./dir` | New file from template (md/python/cpp/html/json/gitignore) |
| `clips image` / `clips path` | Clipboard history filtered by type (text/url/email/path/code/ip/image) |
| `workflow review` / `run review` | Named multi-step workflows (`workflows` lists) |
| `ql docs hallo` / `ticket:ABC-123` | Parameterized quicklinks (`{1}`/`{*}`/`{query}`) |
| `lock` / `sleep` / `shutdown` / `restart` / `logout` / `empty trash` | System commands |
| `wifi` / `wifi off` / `bluetooth toggle` | Wi-Fi + Bluetooth status and on/off (enter applies) |
| `volume 50` / `volume mute` / `brightness 70` | Volume level/mute + display brightness (enter applies) |
| `settings wifi` / `settings sound` / `settings` | Open OS settings pages (wifi/network/bluetooth/sound/display/...) |
| `setup` / `config validate` / `config open` | First-run status + edit/validate wilfred.yml (`wilfred setup`, `config-validate`) |
| `update` / `upgrade` | New-version status from startup check (enter copies link, `wilfred update` installs) |
| `settings edit search.max_results 40` / `settings get hotkey.key` | Update/read one setting in place, validated (`wilfred config-set/get`) |
| `plugins` / `plugins approve demo` | Plugin list + trust approvals (`wilfred plugin list/install/approve`) |
| `layout tile grid` / `tile halves` / `layout werk` | Tile open windows or apply saved layout (`wilfred tile`, `layouts.auto_apply` on monitor change) |
| `winget firefox` / `brew switch` / `apt vlc` | Package search -- Enter installs, `Tab` copies the command (`packages` lists managers) |
| `clip` / `clips` | Clipboard and recent clips |
| `!yt katten` / `gh wilfred` | Search macros (`macros` lists them) |

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
* **Mobile is experimental (as of v46.5.8).** The Android (`.apk`) and iOS (simulator `.app`) builds install and run but are not daily-driver ready pending further testing and development -- deploy desktop first (see `docs/android.md`, `docs/ios.md`).