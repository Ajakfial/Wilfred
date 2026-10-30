# (TO README ZOSTAŁO UTWORZONE PRZEZ CURSOR AI, WSZYSTKO POZOSTAŁE, W TYM KOD, ZOSTAŁO NAPISANE RĘCZNIE PRZEZ CZŁOWIEKA.)

# Wilfred

**Strona internetowa:** https://ajakfial.github.io/Wilfred/

Wilfred to szybki, lekki, wieloplatformowy silnik wyszukiwania i launcher aplikacji dla pulpitu. Został zaprojektowany jako głęboka warstwa wyszukiwania systemu operacyjnego: pliki, foldery, aplikacje, integracja z przeglądarką, kalkulator i filtrowane zapytania z pojedynczego wywoływalnego paska wyszukiwania.

**SZYBKI. GŁĘBOKI. MODUŁOWY. NISKIE ZUŻYCIE ZASOBÓW. WIELPLATFORMOWY. ROZSZERZALNY.**

## Funkcje

* Inkrementalny, trwały indeks nazw plików/ścieżek/metadanych z write-ahead logiem
* Obserwatorzy systemu plików (ReadDirectoryChangesW, FSEvents, inotify) plus fallback re-skoku
* Rozmyte dopasowanie, akronimy, ranking tokenów/ścieżek, sygnały aktualności i częstotliwości
* Bezpieczeństwo literówek: Damerau (z uwzględnieniem transpozycji) near-miss matching na nazwach, tokenach i ścieżkach, plus korekta `Czy chodziło o` (komendy, historia, nazwy plików)
* Wbudowane autouzupełnianie: ghost completion (`→` do akceptacji) + top-N kandydatów z historii, komend i indeksu
* Ranking świadomy kontekstu (ostatnie foldery, typy plików, pora dnia, zapytania sesji)
* Schowek jako źródło drugorzędne (dopasowanie tekstu, skopiowane ścieżki, historia clipów)
* Trwały menedżer schowka (`clips [query]`, przypnij, `clips clear`, `clips url|email|path|code|ip`)
* Indeksowanie treści dla kodu źródłowego, konfiguracji i dokumentów tekstowych
* Prawdziwe wyciąganie tekstu z dokumentów (PDF, Office, RTF, HTML) do indeksu treści
* Opcjonalne wyszukiwanie semantyczne: lokalne embeddingi (serwer/model llama.cpp lub wbudowany hash) + wektorowy indeks HNSW (`vectors.bin` obok WAL/snapshot) scalony z trigramowym soft-match (`providers.semantic`, `embedding.*`)
* Opcjonalny asystent AI (`ai ...`/`ask ...`, ekranowo-świadomy `ai see ...`) z własnymi kluczami: OpenAI, Anthropic, Gemini, Groq
* Snippety z folderami, placeholderami `{date}`/`{clipboard}`/`{query}`, i opcjonalnymi globalnymi skrótami w każdej apce
* Szybkie notatki + zadania (`note ...`, `todo ...`, trwałe w katalogu danych)
* Pomodoro / timery odliczające + stoper (`timer`, `pomodoro`, `stopwatch`)
* Bogate podglądy plików (dokumenty, CSV, obrazy, foldery) w overlaju (`F3`) plus `wilfred preview`
* Kalendarz (`.ics`), kontakty (`.vcf`), i notatki (Markdown) źródła + opcjonalne OCR przez `tesseract` (`sources.*`)
* Cross-platform overlay (Windows WebView2 topmost, macOS floating panel, Linux WebKitGTK z auto X11 canvas fallback, layer-shell zakotwiczony na kompozytorach Wayland, wszystkie above/skip-taskbar) z `Ctrl`/`⌘`+`K` actions popover i pasujących skrótów w obu backendach Linux
* Przeglądarka zakładek, historia i otwarte karty (`bm`, provider-backed)
* Natychmiastowa enumeracja pełnego dysku NTFS przez USN journal na Windows
* Oficjalna federacja indeksu OS (`os_search:`): Windows Search, Spotlight, Tracker/Baloo, `locate` wypełniają luki, które indeks Wilfreda pomija (`os <query>` wymusza, duplikaty odrzucane)
* Paneł podglądu w overlaju (`F3`) plus `wilfred preview`, i łańcuchowe akcje wyników (`open+copy_path`) + nazwane wieloetapowe workflow (`workflows:`)
* Mini-wyniki dla pogody, czasu, dysku, RAM, CPU, procesów, baterii, okien, timerów, notatek, mediów, sieci, layoutów i więcej
* Zarządzanie oknami (`windows`, `minimize`/`maximize`/`close window`, snap) + zapisane układy okien (`layout save`, `layout`)
* Ekranowo-świadomy AI (`ai see ...` wysyła screenshot jako vision input do providera)
* Quick notes teraz żyją jako prawdziwe pliki Markdown, przeszukiwalne zarówno jako karty, jak i przez wyszukiwanie notatek
* System-wide automation: dodatkowe globalne hotkeye (`hotkeys:`), `wilfred workflow` / `wilfred exec` CLI triggers
* Makra wyszukiwania (`!yt`, `gh`, `wiki`, plus custom templates in config) + parameterized quicklinks (`quicklinks:` z `{1}`/`{*}`/`{query}`)
* Per-app extra context actions (`app_actions:`) na wbudowanych file actions
* Large-file finder + duplicate candidates z indeksu (`large`, `dupes`)
* File conversion (`convert song.wav to mp3`, `convert photo.bmp to png`: native WAV/RAW + BMP/PNG/PPM/TGA, ffmpeg for mp3/ogg/flac/jpg/webp) + background removal (`bgremove photo.png` to transparent PNG, no model needed)
* Process killer (`kill <pid|name>`), native media keys (`media play/next/mute/vol` -- no helpers to install), network tools (`ping`, `dns`, `myip`), speech-to-text (`transcribe` via whisper CLI or built-in spoke engine)
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

Requires CMake 3.16+ and a C++20 compiler (MSVC, Apple Clang, or GCC).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

On Linux, X11 development headers enable the overlay (`libx11-dev`); on Wayland sessions the WebKit overlay additionally becomes a native layer-shell surface when `libgtk-layer-shell-dev` is installed (Sway/wlroots, KDE Plasma -- GNOME has no layer-shell protocol, where the window stays a plain toplevel).

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
| `? koty` / `g koty` | Web search in default browser |
| `*.cpp in Projects` | Extension + directory filter |
| `type:image name:logo` | Kind + filename |
| `content:widget` / `intext foo` | Search indexed file contents |
| `> notepad` | Command / launch style |
| `scope:home notatki` | Named directory group from config |
| `pogoda` / `pogoda Warszawa` | Mini card: local or city forecast |
| `speedtest` | Live ping, download, and upload (`speedtest again` reruns) |
| `czas` `dysk` `dysku` `ram` `cpu` | Clock, drives, memory, processor |
| `proces chrome` / `top` | Live process CPU, RAM, threads (`kill <pid|name>` terminates) |
| `zrzut ekranu` / `zrzut ekranu okno` / `zrzut ekranu region` | Capture fullscreen, window, or region to `Pictures/Wilfred` |
| `emoji uśmiech` / `symbol euro` | Emoji and symbol picker (enter copies) |
| `100 usd do eur` / `fx 25 gbp jpy` | Currency conversion |
| `1 łyżka do g` / `1 kubek mąki do g` | Cooking volume↔mass (water default, optional ingredient) |
| `#ff5500` / `rgb(255, 85, 0)` / `kolor coral` | Color conversion (hex / rgb / hsl) |
| `15pm est do pst` / `now in tokyo` / `tz londyn` | Timezones |
| `dzisiaj + 7 dni` / `2024-01-31 + 1 miesiąc` | Date math |
| `uuid` / `base64 cześć` / `sha256 abc` / `lorem 12` / `json {"a":1}` | Dev utilities (enter copies) |
| `hex 255` / `dec 0xff` / `0b1010` / `bit and 12 10` | Number bases + bit tools |
| `regex foo.* foobar` / `urlencode a b` / `jwt <token>` | Regex tester, URL codec, JWT decode |
| `timer 10m` / `pomodoro` / `stopwatch start` | Countdowns, Pomodoro presets, stopwatch |
| `note kup mleko` / `notes` / `todo wyślij` / `todos` | Quick notes + todos (`todo done 1`) |
| `media play` / `media next` / `media mute` | Native media keys + volume (Win media keys; mac HID + Music/Spotify/VLC; Linux native MPRIS D-Bus + wpctl/pactl/amixer, playerctl optional) |
| `ping example.com` / `dns example.com` / `myip` | Ping, DNS lookup, public IP |
| `large 10` / `dupes` | Biggest files + duplicate candidates from index |
| `transcribe rozmowa.mp3` / `dyktuj 10` | Speech-to-text for mp3/mp4 audio, mic dictation (`transcription.engine`: whisper CLI or built-in spoke engine) |
| `convert song.wav to mp3` / `convert photo.bmp to png` | File conversion (native PCM + BMP/PNG/PPM/TGA, ffmpeg for compressed) |
| `bgremove photo.png` / `bgremove photo.png 40 #ffffff` | Background removal to transparent PNG (auto corners or explicit color) |
| `okna chrome` / `zminimalizuj spotify` | List, switch, minimize, maximize, close, snap windows |
| `layout save praca` / `layout praca` | Save and restore window layouts |
| `define resilient` / `thesaurus happy` | Offline definition + synonyms (enter copies) |
| `pins` / `pin firefox` / `unpin firefox` | Pinned favorites (boosted via `ranking.pinned`) |
| `event add Spotkanie zespołu | jutro 10am` | Create a calendar event (writes `.ics`) |
| `contact add Jan Kowalski jan@x.com 555-0100` | Create a contact (writes `.vcf`) |
| `rename ./zdjecia zdjecie-{n}.jpg` | Bulk rename with `{n}` `{name}` `{ext}` |
| `template python foo in ./dir` | New file from template (md/python/cpp/html/json/gitignore) |
| `clips image` / `clips path` | Clipboard history filtered by type (text/url/email/path/code/ip/image) |
| `workflow przeglad` / `run przeglad` | Named multi-step workflows (`workflows` lists) |
| `ql docs cześć` / `ticket:ABC-123` | Parameterized quicklinks (`{1}`/`{*}`/`{query}`) |
| `lock` / `sleep` / `shutdown` / `restart` / `logout` / `empty trash` | System commands |
| `wifi` / `wifi off` / `bluetooth toggle` | Wi-Fi + Bluetooth status and on/off (enter applies) |
| `volume 50` / `volume mute` / `brightness 70` | Volume level/mute + display brightness (enter applies) |
| `settings wifi` / `settings sound` / `settings` | Open OS settings pages (wifi/network/bluetooth/sound/display/...) |
| `setup` / `config validate` / `config open` | First-run status + edit/validate wilfred.yml (`wilfred setup`, `config-validate`) |
| `update` / `upgrade` | New-version status from startup check (enter copies link, `wilfred update` installs) |
| `settings edit search.max_results 40` / `settings get hotkey.key` | Update/read one setting in place, validated (`wilfred config-set/get`) |
| `plugins` / `plugins approve demo` | Plugin list + trust approvals (`wilfred plugin list/install/approve`) |
| `layout tile grid` / `tile halves` / `layout praca` | Tile open windows or apply saved layout (`wilfred tile`, `layouts.auto_apply` on monitor change) |
| `winget firefox` / `brew switch` / `apt vlc` | Package search -- Enter installs, `Tab` copies the command (`packages` lists managers) |
| `clip` / `clips` | Clipboard and recent clips |
| `!yt koty` / `gh wilfred` | Search macros (`macros` lists them) |

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
| Snapshot save/load: 10K | Persistence round-trip |
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