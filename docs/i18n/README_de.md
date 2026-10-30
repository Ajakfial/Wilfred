# (DIESE README IST VON CURSOR AI ERSTELLT, ALLES ANDERE, EINSCHLIESSLICH CODE, IST VON MENSCHLICHER HAND GESCHRIEBEN.)

# Wilfred

**Website:** https://ajakfial.github.io/Wilfred/

Wilfred ist eine schnelle, leichte, plattformübergreifende Desktop-Suchmaschine und Anwendungsstarter. Sie ist als tiefe OS-Suchschicht konzipiert: Dateien, Ordner, Anwendungen, Browser-Integration, Rechner und gefilterte Abfragen aus einer einzigen aufrufbaren Suchleiste.

**SCHNELL. TIEF. MODULAR. RESSOURCENSCHONEND. PLATTFORMÜBERGREIFEND. ERWEITERBAR.**

## Funktionen

* Inkrementeller, persistenter Dateinamen/Pfad/Metadaten-Index mit Write-Ahead-Log
* Dateisystem-Beobachter (ReadDirectoryChangesW, FSEvents, inotify) plus Rescan-Fallback
* Fuzzy-Matching, Akronyme, Token/Pfad-Ranking, Aktualitäts- und Häufigkeitssignale
* Tippfehler-Sicherheit: Damerau (transpositionsbewusstes) Near-Miss-Matching bei Namen, Tokens und Pfaden plus `Meinten Sie`-Korrektur (Befehle, Verlauf, Dateinamen)
* Inline-Autovervollständigung: Geistervervollständigung (`→` zum Akzeptieren) + Top-N-Kandidaten aus Verlauf, Befehlen und Index
* Kontextbewusstes Ranking (kürzlich verwendete Ordner, Dateitypen, Tageszeit, Sitzungsabfragen)
* Zwischenablage als sekundäre Quelle (Textsuche, kopierte Pfade, Clip-Verlauf)
* Persistenter Zwischenablage-Manager (`clips [Abfrage]`, anheften, `clips clear`, `clips url|email|path|code|ip`)
* Inhaltsindizierung für Quellcode, Konfiguration und Textdokumente
* Echte Dokumenttextextraktion (PDF, Office, RTF, HTML) in den Inhaltsindex
* Optionale semantische Suche: lokale Embeddings (llama.cpp Server/Modell oder eingebauter Hash) + HNSW-Vektorindex (`vectors.bin` neben WAL/Snapshot) vereint mit Trigramm-Soft-Match (`providers.semantic`, `embedding.*`)
* Optionaler KI-Assistent (`ai ...`/`ask ...`, bildschirmbewusstes `ai see ...`) mit eigenen Schlüsseln: OpenAI, Anthropic, Gemini, Groq
* Snippets mit Ordnern, `{date}`/`{clipboard}`/`{query}`-Platzhaltern und optionalen globalen Abkürzungen in jeder App
* Schnelle Notizen + Todos (`note ...`, `todo ...`, persistent im Datenverzeichnis)
* Pomodoro / Countdown-Timer + Stoppuhr (`timer`, `pomodoro`, `stopwatch`)
* Reiche Dateivorschauen (Dokumente, CSV, Bilder, Ordner) im Overlay (`F3`) plus `wilfred preview`
* Kalender (`.ics`), Kontakte (`.vcf`) und Notizen (Markdown) Quellen + optionale OCR-Indizierung via `tesseract` (`sources.*`)
* Plattformübergreifendes Overlay (Windows WebView2 topmost, macOS schwebendes Panel, Linux WebKitGTK mit automatischem X11-Canvas-Fallback, Layer-Shell-verankert auf Wayland-Compositoren, alle oben/skip-taskbar) mit `Ctrl`/`⌘`+`K` Aktionen-Popover und passenden Shortcuts in beiden Linux-Backends
* Browser-Lesezeichen, Verlauf und offene Tabs (`bm`, provider-gestützt)
* Sofortige NTFS-Vollfestplattenaufzählung über das USN-Journal unter Windows
* Offizieller OS-Index-Verbund (`os_search:`): Windows Search, Spotlight, Tracker/Baloo, `locate` füllen Lücken, die Wilfreds Index verpasst (`os <Abfrage>` erzwingt es, Duplikate verworfen)
* Vorschau-Pane im Overlay (`F3`) plus `wilfred preview`, und verkettete Ergebnisaktionen (`open+copy_path`) + benannte mehrstufige Workflows (`workflows:`)
* Mini-Ergebnisse für Wetter, Zeit, Festplatte, RAM, CPU, Prozesse, Batterie, Fenster, Timer, Notizen, Medien, Netzwerk, Layouts und mehr
* Fensterverwaltung (`windows`, `minimize`/`maximize`/`close window`, snap) plus gespeicherte Fensterlayouts (`layout save`, `layout`)
* Bildschirmbewusste KI (`ai see ...` sendet Screenshot als Vision-Eingabe an Ihren Provider)
* Schnelle Notizen leben jetzt als echte Markdown-Dateien, durchsuchbar sowohl als Karten als auch über Notizsuche
* Systemweite Automatisierung: zusätzliche globale Hotkeys (`hotkeys:`), `wilfred workflow` / `wilfred exec` CLI-Auslöser
* Suchmakros (`!yt`, `gh`, `wiki`, plus benutzerdefinierte Vorlagen in der Konfiguration) + parametrisierte Schnelllinks (`quicklinks:` mit `{1}`/`{*}`/`{query}`)
* Pro-App zusätzliche Kontextaktionen (`app_actions:`) zusätzlich zu den eingebauten Dateiaktionen
* Großdatei-Finder + Duplikatkandidaten aus dem Index (`large`, `dupes`)
* Dateikonvertierung (`convert song.wav to mp3`, `convert photo.bmp to png`: natives WAV/RAW + BMP/PNG/PPM/TGA, ffmpeg für mp3/ogg/flac/jpg/webp) + Hintergrundentfernung (`bgremove photo.png` zu transparentem PNG, kein Modell nötig)
* Prozesskiller (`kill <pid|name>`), native Mediatasten (`media play/next/mute/vol` -- keine Helfer zu installieren), Netzwerktools (`ping`, `dns`, `myip`), Sprache-zu-Text (`transcribe` via whisper CLI oder eingebaute spoke Engine)
* Komponierbare Filter (`*.cpp in Projects`, `type:image`, `size:>10mb`, benannte Bereiche)
* Anwendungsentdeckung (Startmenü / `.app` Bundles / `.desktop` Dateien)
* Reiche Dateiaktionen (Kopierpfad-Varianten, SHA-256 Hash, zip, Terminal/Editor hier, neue Datei/Ordner, Öffnen mit)
* Standardbrowser-Erkennung, URL-Öffnen und Websuch-Fallback
* Sicherer Ausdrucksparser (Arithmetik, Funktionen, Einheiten/Währungs/Farben/Datum-Konvertierung, uuid/base64/sha256/lorem/json, hex/dec/bin/oct, Bits, regex/url/jwt)
* Native und stdio Plugins, plus zusätzliche Suchbackends über Provider
* Rechner plus Einheiten, Währungen, Farben, Datum und kleine Dev-Utilities
* Systembefehle aus dem Overlay (`lock`, `sleep`, `shutdown`, `restart`, `logout`, Papierkorb leeren)
* Systemumschalter (`wifi on/off`, `bluetooth toggle`, `volume 50/mute`, `brightness 70`) + Einstellungen Deep-Links (`settings wifi/bluetooth/sound/display/...`) -- echte OS-Backends unter Windows/macOS/Linux, ehrliche "nicht unterstützt" Fehler bei Mobile/BSD-Lücken
* Paketmanager-Suche (`winget firefox`, `brew switch`, `apt vlc` -- Enter installiert, `Tab` kopiert den Befehl; `packages` listet erkannte Manager)
* Einstellungen + Setup (`settings config open/validate`, `settings edit <key> <value>`, `setup`, `wilfred setup --overwrite`, `wilfred config-get/set/reset`, `wilfred config-validate/open/path`) und optionale Remote-Suchbackends (`remotes:` -- nur Ihre Endpunkte, standardmäßig aus; pro-Quelle `headers:` + `max_results:`)
* Plugins mit Vertrauen bei erstem Gebrauch (`plugins` Freigabekarten, `wilfred plugin list/install/approve/revoke`, optionale `plugins.registry` Index, sha256-verifizierte Installationen)
* Fenstertiling (`layout tile halves|thirds|grid|columns N|rows N|stack`, `wilfred tile …`) + Monitorwechsel Auto-Anwendung (`layouts.auto_apply`, `auto_layout`, `monitor_layouts`)
* Android App (Kotlin UI + C++ Kern via NDK → `.apk`): schwebender W-Button, Suchleiste, gleicher Index/Suche/Ranking -- siehe `docs/android.md`
* iOS App (SwiftUI + gleicher C++ Kern → Simulator `.app`): gleiche Such-UI wie Android (App-Icon-Einstieg; iOS verbietet Overlay-Buttons), Sandbox-nur Index -- siehe `docs/ios.md`
* Lokale Backup/Wiederherstellung und optionale Remote-Sync (`backup`, `restore`, `sync-push`, `sync-pull`)
* Import aus anderen Launchern (`wilfred import --list/--detect/auto --dry-run/--overwrite`): Alfred, Raycast, PowerToys Run, Flow Launcher/Wox, Keypirinha, Listary, Ulauncher, Albert, KRunner, Rofi -- siehe `docs/import.md`
* YAML-Konfiguration mit Validierung, `Meinten Sie`-Hinweise und menschenlesbaren Fehlern
* Globaler Hotkey: `Ctrl+Alt+W` (Windows/Linux), `⌘+Alt+W` (macOS), konfigurierbar
* Lokaler Suchverlauf (optional; deaktivierbar oder `wilfred history-clear`)
* Overlay-UI plus CLI (`search`, `launch`, `index`, `status`, backup/sync, `import`)

## Build

Benötigt CMake 3.16+ und einen C++20-Compiler (MSVC, Apple Clang oder GCC).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Unter Linux aktivieren X11-Entwicklungsheader das Overlay (`libx11-dev`); unter Wayland-Sitzungen wird das WebKit-Overlay zusätzlich zu einer nativen Layer-Shell-Oberfläche, wenn `libgtk-layer-shell-dev` installiert ist (Sway/wlroots, KDE Plasma -- GNOME hat kein Layer-Shell-Protokoll, dort bleibt das Fenster ein normales Toplevel).

## Ausführen

```bash
# Hintergrund-Daemon: Overlay, Indexer, Hotkey
wilfred

# Einmalige Suche gegen den lokalen Index
wilfred search firefox

# Suche und öffne das Top-Ergebnis
wilfred launch firefox

# Indiziere konfigurierte Roots jetzt
wilfred index

# Statistiken
wilfred status

# Importiere Einstellungen von einem anderen Launcher
wilfred import --list
wilfred import --detect
wilfred import auto --dry-run
wilfred import alfred --from Alfred.alfredpreferences --dry-run
```

Benutzerkonfiguration wird beim ersten Start erstellt:

| Plattform | Konfig | Indexdaten |
|---|---|---|
| Windows | `%APPDATA%\Wilfred\wilfred.yml` | `%LOCALAPPDATA%\Wilfred\` |
| macOS | `~/Library/Application Support/Wilfred/` | same |
| Linux | `~/.config/wilfred/wilfred.yml` | `~/.local/share/wilfred/` |
| BSD | `~/.config/wilfred/wilfred.yml` | `~/.local/share/wilfred/` |

Siehe `config/wilfred.default.yml` für das vollständige Schema (Index-Roots, Ausschlüsse, Systemverzeichnisse, Ranking-Gewichte, Aliase, Hotkey, Verlauf, Browser).

## Migrieren von einem anderen Launcher

`wilfred import` bringt Hotkeys, benutzerdefinierte Websuchen (als `macros:` + `quicklinks:`), Snippets, Aliase, Theme und das Standard-Such-Template in `wilfred.yml`. Zusammenführung ist Standard; `--overwrite` ersetzt Konflikte, `--dry-run` zeigt Vorschau, und eine `.pre-import.bak`-Sicherung wird vor jedem echten Import geschrieben.

| OS | Unterstützte Quellen |
|---|---|
| macOS | Alfred (`Alfred.alfredpreferences` Bundle), Raycast (Schnelllinks JSON) |
| Windows | PowerToys Run, Flow Launcher, Wox, Keypirinha, Listary |
| Linux | Ulauncher, Albert, KRunner / KDE, Rofi |

Siehe `docs/import.md` (und `wilfred import --list`) für pro-Launcher-Zuordnung.

## Abfragesprache (Beispiele)

| Abfrage | Bedeutung |
|---|---|
| `firefox` | Fuzzy Datei/App-Suche |
| `25 * 42` | Rechner |
| `https://example.com` | URL öffnen |
| `? katzen` / `g katzen` | Websuche im Standardbrowser |
| `*.cpp in Projects` | Erweiterung + Verzeichnis-Filter |
| `type:image name:logo` | Art + Dateiname |
| `content:widget` / `intext foo` | Indizierte Dateiinhalte suchen |
| `> notepad` | Befehl / Start-Stil |
| `scope:home notizen` | Benannte Verzeichnisgruppe aus der Konfig |
| `wetter` / `wetter Berlin` | Mini-Karte: lokal oder Stadtvorhersage |
| `speedtest` | Live Ping, Download und Upload (`speedtest again` wiederholt) |
| `zeit` `disk` `disku` `ram` `cpu` | Uhr, Laufwerke, Speicher, Prozessor |
| `prozess chrome` / `top` | Live Prozess CPU, RAM, Threads (`kill <pid|name>` beendet) |
| `screenshot` / `screenshot window` / `screenshot region` | Erfasse Vollbild, Fenster oder Region nach `Pictures/Wilfred` |
| `emoji smile` / `symbol euro` | Emoji- und Symbolauswahl (Enter kopiert) |
| `100 usd to eur` / `fx 25 gbp jpy` | Währungsumrechnung |
| `1 tbsp to g` / `1 cup flour to g` | Kochvolumen↔Masse (Wasser Standard, optionale Zutat) |
| `#ff5500` / `rgb(255, 85, 0)` / `farbe coral` | Farbkonvertierung (hex / rgb / hsl) |
| `3pm est to pst` / `now in tokyo` / `tz london` | Zeitzonen |
| `today + 7 days` / `2024-01-31 + 1 month` | Datumsrechnung |
| `uuid` / `base64 hi` / `sha256 abc` / `lorem 12` / `json {"a":1}` | Dev-Utilities (Enter kopiert) |
| `hex 255` / `dec 0xff` / `0b1010` / `bit and 12 10` | Zahlbasen + Bit-Tools |
| `regex foo.* foobar` / `urlencode a b` / `jwt <token>` | Regex-Tester, URL-Codec, JWT-Dekoder |
| `timer 10m` / `pomodoro` / `stopwatch start` | Countdowns, Pomodoro-Vorgaben, Stoppuhr |
| `note milch kaufen` / `notes` / `todo versenden` / `todos` | Schnelle Notizen + Todos (`todo done 1`) |
| `media play` / `media next` / `media mute` | Native Mediatasten + Lautstärke (Win Mediatasten; mac HID + Music/Spotify/VLC; Linux natives MPRIS D-Bus + wpctl/pactl/amixer, playerctl optional) |
| `ping example.com` / `dns example.com` / `myip` | Ping, DNS-Lookup, öffentliche IP |
| `large 10` / `dupes` | Größte Dateien + Duplikatkandidaten aus dem Index |
| `transcribe talk.mp3` / `dictate 10` | Sprache-zu-Text für mp3/mp4-Audio, Mikrofon-Diktat (`transcription.engine`: whisper CLI oder eingebaute spoke Engine) |
| `convert song.wav to mp3` / `convert photo.bmp to png` | Dateikonvertierung (natives PCM + BMP/PNG/PPM/TGA, ffmpeg für komprimierte) |
| `bgremove photo.png` / `bgremove photo.png 40 #ffffff` | Hintergrundentfernung zu transparentem PNG (Auto-Ecken oder explizite Farbe) |
| `windows chrome` / `minimize spotify` | Fenster auflisten, wechseln, minimieren, maximieren, schließen, snap |
| `layout save arbeit` / `layout arbeit` | Fensterlayouts speichern und wiederherstellen |
| `define resilient` / `thesaurus happy` | Offline Definition + Synonyme (Enter kopiert) |
| `pins` / `pin firefox` / `unpin firefox` | Gepinnte Favoriten (gestärkt via `ranking.pinned`) |
| `event add Team sync | morgen 10am` | Kalenderevent erstellen (schreibt `.ics`) |
| `contact add Jane Doe jane@x.com 555-0100` | Kontakt erstellen (schreibt `.vcf`) |
| `rename ./photos foto-{n}.jpg` | Massenumbenennung mit `{n}` `{name}` `{ext}` |
| `template python foo in ./dir` | Neue Datei aus Vorlage (md/python/cpp/html/json/gitignore) |
| `clips image` / `clips path` | Zwischenablageverlauf gefiltert nach Typ (text/url/email/path/code/ip/image) |
| `workflow review` / `run review` | Benannte mehrstufige Workflows (`workflows` listet) |
| `ql docs hello` / `ticket:ABC-123` | Parametrisierte Schnellinks (`{1}`/`{*}`/`{query}`) |
| `lock` / `sleep` / `shutdown` / `restart` / `logout` / `empty trash` | Systembefehle |
| `wifi` / `wifi off` / `bluetooth toggle` | Wi-Fi + Bluetooth Status und an/aus (Enter wendet an) |
| `volume 50` / `volume mute` / `brightness 70` | Lautstärke Pegel/Stumm + Display-Helligkeit (Enter wendet an) |
| `settings wifi` / `settings sound` / `settings` | OS-Einstellungsseiten öffnen (wifi/network/bluetooth/sound/display/...) |
| `setup` / `config validate` / `config open` | Ersteinrichtungsstatus + wilfred.yml bearbeiten/validieren (`wilfred setup`, `config-validate`) |
| `update` / `upgrade` | Neuversion-Status vom Startcheck (Enter kopiert Link, `wilfred update` installiert) |
| `settings edit search.max_results 40` / `settings get hotkey.key` | Einstellung an Ort und Stelle aktualisieren/lesen, validiert (`wilfred config-set/get`) |
| `plugins` / `plugins approve demo` | Plugin-Liste + Vertrauensfreigaben (`wilfred plugin list/install/approve`) |
| `layout tile grid` / `tile halves` / `layout work` | Offene Fenster kacheln oder gespeichertes Layout anwenden (`wilfred tile`, `layouts.auto_apply` bei Monitorwechsel) |
| `winget firefox` / `brew switch` / `apt vlc` | Paketsuche -- Enter installiert, `Tab` kopiert den Befehl (`packages` listet Manager) |
| `clip` / `clips` | Zwischenablage und letzte Clips |
| `!yt katzen` / `gh wilfred` | Suchmakros (`macros` listet sie) |

## Architektur

Plattform-I/O lebt hinter `wilfred/platform/native.hpp`. Kernmodule:

* `config` -- YAML-Parse + Schema-Validierung
* `index` -- String-Intern-Pool, kompakte Records, Inverted/Trigramm-Indizes, WAL
* `fs` -- Walker, Klassifizierung, Volumes, Watcher
* `search` -- Fuzzy, Ranking-Gewichte, Filter, Abfrage-Cache
* `query` -- Klassifizierung + Interpreter
* `math` -- Rekursiver-Abwärts-Rechner
* `apps` / `browser` / `history` / `hotkey` / `ipc` / `ui` / `service`
* `plugin` -- Native `.dll`/`.so`/`.dylib` ABI und stdio Plugins, Vertrauensspeicher, Registry-Index
* `sync` -- Lokale Backup-Archive und optionale Remote Push/Pull
* `import` -- Cross-OS Launcher-Einstellungen-Importer (Hotkeys, Suchen, Snippets)
* `providers` -- Registry für zusätzliche Suchbackends (semantisch, Browser-Bibliothek, Kalender/Kontakte/Notizen, Remotes, Pakete)

## Tests und Benchmarks

```bash
cmake --build build --config Release --target wilfred_tests wilfred_bench
./build/wilfred_tests          # oder build/Release/wilfred_tests.exe
./build/wilfred_bench
```

Tests decken Index CRUD, Persistenz/Wiederherstellung, Fuzzy/Ranking/Filter, Mathe, Abfrageklassifizierung, Konfigurationsvalidierung, Verlauf, Pfade, IPC und Anwendungsentdeckung ab.

## Leistung

Wilfred ist ein einzelnes C++20-Binary ohne externe Drittanbieterbibliotheken -- alles (JSON, YAML, mmap-Wrapper, CRC32) ist im Baum implementiert, daher gibt es nichts Extra zu installieren (das Linux-Overlay nutzt systemseitige X11/WebKit-Bibliotheken falls vorhanden, plus gtk-layer-shell für natives Wayland-Verankern, mit X11-Canvas-Fallback). Die Geschwindigkeit kommt vom Index-Design (siehe `docs/architecture.md`):

* Kompakter memory-mapped Record-Speicher mit String-Intern-Pool (keine doppelten Pfad/Name-Strings im RAM)
* Trigramm + Token-Indizes, damit Abfragen Posting-Listen treffen, keine Vollscans
* Write-Ahead-Log mit periodischen Snapshots und Absturz-Wiederherstellung -- Schreibvorgänge sind Appends, Lesen blockiert nie auf Persistenz
* Inkrementelle, entprellte Dateisystem-Beobachter-Updates statt Rescans
* Begrenzte Ergebnismengen (Top-40) mit Pro-Abfrage-Timeouts bei Plugins

`wilfred_bench` (`benches/bench_main.cpp`) misst die Hotpaths auf synthetischen 1K-100K Dateinamen-Datensätzen (10 zeitgesteuerte Iterationen + 1 Warmup, 5 für die 100K-Einfügung) und berichtet Min / Median / Mean / P95 / Max Millisekunden:

| Benchmark | Was es misst |
|---|---|
| String Pool: intern 100K | Interner Durchsatz (Index Speichereffizienz) |
| Tokenizer: tokenisiere 100K | Dateinamen-Tokenisierung |
| Index Einfügung: 1K / 10K / 100K | Massenindex-Aufbau |
| Index Abfrage: 10K / 100K Records | Posting-Listen-Lookup |
| Fuzzy-Suche: 10K Ziele | Damerau-bewusster Scoring-Durchsatz |
| Ranking: 10K bewertete Treffer | Gewichteter Scoring + Top-40 Auswahl |
| Filterkette: 10K Records | Erweiterungs/Art-Filter-Auswertung |
| Mathe-Auswertung: 1K Ausdrücke | Rechner Parse + Eval |
| Snapshot speichern / laden: 10K | Persistenz Round-Trip |
| Speicherbericht: 100K Index | Index-Footprint-Accounting |

Führen Sie es auf Ihrer eigenen Hardware aus, bevor Sie Zahlen zitieren -- Ergebnisse hängen von CPU, Disk und Datensatzform ab:

```bash
cmake --build build --config Release --target wilfred_bench
./build/wilfred_bench          # oder build/Release/wilfred_bench.exe
```

## Verpackung

Ein Push eines `v*.*.*` Tags startet `.github/workflows/release.yml`, welches Release-Binaries baut und ein Archiv pro Plattform an den GitHub Release anhängt (siehe `docs/building.md#packaging-was-ci-tut`):

| Job | Artefakte |
|---|---|
| `windows-x64` | Portables `.zip`, signiertes `.msi` (WiX v3, pro-Benutzer, kein UAC), Chocolatey `.nupkg` (eingebettetes Portable, kein Download bei Installation) |
| `linux-x64` | Portables `.tar.gz` |
| `freebsd-x64` | Portables `.tar.gz` (FreeBSD Release-Job; andere BSDs Bauen aus Quelle -- siehe `docs/bsd.md`) |
| `macos-arm64` | Portables `.tar.gz` |
| `android` | `.apk` + `.aab` -- signiert wenn Keystore-Secrets existieren, sonst `-unsigned` suffixed (siehe `docs/android.md`) |
| `ios` | Unsigniertes Simulator `.zip` (siehe `docs/ios.md`) |

Desktop-Archive legen das Binary mit `ui/overlay/`, `README.md` und `config/wilfred.default.yml` ab. Windows-Binaries tragen immer den Publisher-Stempel `Wilfred Open Contributors` via VERSIONINFO; Authenticode-Vertrauen kommt von PFX oder Azure Trusted Signing Secrets wenn konfiguriert (siehe `docs/signing.md`, `docs/installer.md`).

## Produktionsbereitstellung

* **Führen Sie den Daemon aus, nicht One-Shots.** `wilfred daemon` (bloßes `wilfred`) ist der langlebige Prozess: Indexer + Watcher + Hotkey + Overlay + IPC-Server. Führen Sie ihn als Login-Item / Autostart-Eintrag aus (`wilfred autostart enable` registriert dieses Binary unter Windows/macOS/Linux/BSD); das Tray-Icon (Windows) beendet es. CLI-Subcommands (`search`, `launch`, `status`, …) sprechen mit dem laufenden Daemon über den lokalen IPC-Socket wenn verfügbar (siehe `docs/cli.md`, `docs/ipc-and-api.md`).
* **Konfig ist Code.** Erster Lauf schreibt ein validiertes `wilfred.yml` (Standorttabelle oben); ungültige Schlüssel scheitern mit menschenlesbaren Fehlern und `Meinten Sie`-Hinweisen, nie stillschweigende Defaults. Für Flotten verteilen Sie ein Baseline-`wilfred.yml` (Roots, Ausschlüsse, Gewichte, Aliase -- vollständiges Schema in `docs/configuration.md` und `config/wilfred.default.yml`) und lassen Benutzer lokal überschreiben.
* **Daten und DR.** Index, Verlauf, Clips, Notizen und Logs leben im pro-OS-Datenverzeichnis (Tabelle oben). `backup` / `restore` verschieben den Zustand einer Maschine; `sync-push` / `sync-pull` replizieren ihn (siehe `docs/sync-and-backup.md`). Testen Sie Wiederherstellungen bevor Sie sie brauchen.
* **Beobachten Sie es.** `wilfred status` meldet Index-Gesundheit; `logging.*` Keys steuern Log-Verbosität/Aufbewahrung; die optionale lokale HTTP-API (`api.*`, authentifiziert) exponiert `/search`, `/status`, `/index`, `/launch` und Backup/Sync-Endpunkte für Dashboards und Automatisierung.
* **Datenschutz-Haltung.** Lokal-erst: kein Konto, keine Telemetrie, keine Netzwerkaufrufe außer Sie aktivieren sie (Wetter/KI/Sync). Deaktivieren oder begrenzen Sie Verlauf via `history.*` wo Richtlinien es erfordern; KI-Provider benötigen Benutzereigene API-Keys (`ai.*`, standardmäßig aus).
* **Mobile ist experimentell (seit v46.5.8).** Die Android (`.apk`) und iOS (Simulator `.app`) Builds installieren und laufen, sind aber noch nicht Daily-Driver-tauglich, da weitere Tests und Entwicklung ausstehen -- deployen Sie zuerst Desktop (siehe `docs/android.md`, `docs/ios.md`).