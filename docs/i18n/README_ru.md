# (ЭТО README СОЗДАНО CURSOR AI, ВСЕ ОСТАЛЬНОЕ, ВКЛЮЧАЯ КОД, НАПИСАНО ЛЮДСКОЙ РУКОЙ.)

# Wilfred

**Веб-сайт:** https://ajakfial.github.io/Wilfred/

Wilfred — это быстрый, лёгкий, кроссплатформенный поисковик для десктопа и запускатель приложений. Он спроектирован как глубокий слой поиска ОС: файлы, папки, приложения, интеграция с браузером, калькулятор и отфильтрованные запросы из единственной вызываемой поисковой строки.

**БЫСТРЫЙ. ГЛУБОКИЙ. МОДУЛЬНЫЙ. НИЗКИЕ РЕСУРСЫ. КРОССПЛАТФОРМЕННЫЙ. РАСШИРЯЕМЫЙ.**

## Возможности

* Инкрементальный, персистентный индекс имён файлов/путей/метаданных с write-ahead log
* Файловые наблюдатели (ReadDirectoryChangesW, FSEvents, inotify) и fallback-ресканирование
* Нечёткий поиск, акронимы, токен/путь ранжирование, сигналы свежести и частоты
* Защита от опечаток: Damerau (с учётом транспозиций) near-miss matching по именам, токенам и путям, плюс коррекция `Возможно, вы имели в виду` (команды, история, имена файлов)
* Встроенный автодополнение: призрачное завершение (`→` для принятия) + топ-N кандидатов из истории, команд и индекса
* Контекстно-зависимое ранжирование (недавние папки, типы файлов, время суток, запросы сессии)
* Буфер обмена как вторичный источник (текстовый поиск, скопированные пути, история клипов)
* Постоянный менеджер буфера обмена (`clips [query]`, закрепление, `clips clear`, `clips url|email|path|code|ip`)
* Индексация содержимого для исходников, конфигов и текстовых документов
* Реальное извлечение текста документов (PDF, Office, RTF, HTML) в контентный индекс
* Опциональный семантический поиск: локальные эмбеддинги (llama.cpp сервер/модель или встроенный хеш) + HNSW векторный индекс (`vectors.bin` рядом с WAL/снимком) объединённый с триграммным soft-match (`providers.semantic`, `embedding.*`)
* Опциональный AI-ассистент (`ai ...`/`ask ...`, экранно-зависимый `ai see ...`) с вашими ключами: OpenAI, Anthropic, Gemini, Groq
* Сниппеты с папками, плейсхолдерами `{date}`/`{clipboard}`/`{query}` и опциональными глобальными аббревиатурами в любом приложении
* Быстрые заметки + задачи (`note ...`, `todo ...`, постоянные в директории данных)
* Помодоро / таймеры обратного отсчёта + секундомер (`timer`, `pomodoro`, `stopwatch`)
* Богатые превью файлов (документы, CSV, изображения, папки) в оверлее (`F3`) и `wilfred preview`
* Календарь (`.ics`), контакты (`.vcf`) и заметки (Markdown) источники + опциональный OCR через `tesseract` (`sources.*`)
* Кроссплатформенный оверлей (Windows WebView2 topmost, macOS плавающая панель, Linux WebKitGTK с авто-фоллбэком на X11 canvas, layer-shell якорь на Wayland композиторах, все above/skip-taskbar) с `Ctrl`/`⌘`+`K` actions popover и соответствующими шорткатами в обоих Linux-бэкендах
* Браузерные закладки, история и открытые вкладки (`bm`, provider-backed)
* Мгновенное перечисление всего NTFS-диска через USN journal на Windows
* Официальная федерация OS-индекса (`os_search:`): Windows Search, Spotlight, Tracker/Baloo, `locate` заполняют пробелы, которые пропускает индекс Wilfred (`os <query>` форсирует, дубликаты отбрасываются)
* Панель предпросмотра в оверлее (`F3`) плюс `wilfred preview`, и цепочечные действия результатов (`open+copy_path`) + именованные многошаговые воркфлоу (`workflows:`)
* Мини-результаты для погоды, времени, диска, RAM, CPU, процессов, батареи, окон, таймеров, заметок, медиа, сети, лейаутов и др.
* Управление окнами (`windows`, `minimize`/`maximize`/`close window`, snap) + сохранённые лэйауты окон (`layout save`, `layout`)
* Экранно-зависимый AI (`ai see ...` отправляет скриншот как vision-ввод провайдеру)
* Quick notes теперь живут как настоящие Markdown-файлы, поискаемые и как карточки, и через поиск заметок
* Системная автоматизация: дополнительные глобальные хоткеи (`hotkeys:`), `wilfred workflow` / `wilfred exec` CLI-триггеры
* Поисковые макросы (`!yt`, `gh`, `wiki`, плюс кастомные шаблоны в конфиге) + параметризованные quicklinks (`quicklinks:` с `{1}`/`{*}`/`{query}`)
* Контекстные действия для конкретных приложений (`app_actions:`) поверх встроенных файловых действий
* Поиск больших файлов + кандидаты дубликатов из индекса (`large`, `dupes`)
* Конвертация файлов (`convert song.wav to mp3`, `convert photo.bmp to png`: нативный WAV/RAW + BMP/PNG/PPM/TGA, ffmpeg для mp3/ogg/flac/jpg/webp) + удаление фона (`bgremove photo.png` в прозрачный PNG, модель не нужна)
* Процесс-киллер (`kill <pid|name>`), нативные медиа-клавиши (`media play/next/mute/vol` -- хелперы не нужны), сетевые инструменты (`ping`, `dns`, `myip`), речь-в-текст (`transcribe` через whisper CLI или встроенный spoke-движок)
* Компонуемые фильтры (`*.cpp in Projects`, `type:image`, `size:>10mb`, named scopes)
* Обнаружение приложений (Start Menu / `.app` бандлы / `.desktop` файлы)
* Богатые файловые действия (варианты копии пути, SHA-256 хеш, zip, терминал/эдитор здесь, новый файл/папка, Open With)
* Определение дефолтного браузера, открытие URL, веб-поиск фоллбэк
* Безопасный парсер выражений (арифметика, функции, единицы/валюта/цвет/дата конвертация, uuid/base64/sha256/lorem/json, hex/dec/bin/oct, биты, regex/url/jwt)
* Нативные и stdio плагины, плюс дополнительные поисковые бэкенды через провайдеры
* Калькулятор с единицами, валютами, цветами, датами и мелкими dev-утилитами
* Системные команды из оверлея (`lock`, `sleep`, `shutdown`, `restart`, `logout`, очистить корзину)
* Системные тогглы (`wifi on/off`, `bluetooth toggle`, `volume 50/mute`, `brightness 70`) + settings deep-links (`settings wifi/bluetooth/sound/display/...`) -- настоящие OS-бэкенды на Windows/macOS/Linux, честные ошибки unsupported на mobile/BSD
* Поиск пакетных менеджеров (`winget firefox`, `brew switch`, `apt vlc` -- Enter устанавливает, `Tab` копирует команду; `packages` листит обнаруженные менеджеры)
* Настройки + сетап (`settings config open/validate`, `settings edit <key> <value>`, `setup`, `wilfred setup --overwrite`, `wilfred config-get/set/reset`, `wilfred config-validate/open/path`) и опциональные удалённые поисковые бэкенды (`remotes:` -- ваши эндпоинты, off по умолчанию; per-source `headers:` + `max_results:`)
* Плагины с trust-on-first-use (`plugins` approval cards, `wilfred plugin list/install/approve/revoke`, опциональный `plugins.registry` индекс, sha256-верифицированные установки)
* Оконный тайлинг (`layout tile halves|thirds|grid|columns N|rows N|stack`, `wilfred tile …`) + авто-применение при смене монитора (`layouts.auto_apply`, `auto_layout`, `monitor_layouts`)
* Android-приложение (Kotlin UI + C++ core via NDK → `.apk`): плавающая W-кнопка, поисковая строка, тот же индекс/поиск/ранкинг -- см. `docs/android.md`
* iOS-приложение (SwiftUI + тот же C++ core → simulator `.app`): тот же поисковый UI как Android (app-icon entry; iOS запрещает overlay-кнопки), sandbox-only индекс -- см. `docs/ios.md`
* Локальный backup/restore и опциональный remote sync (`backup`, `restore`, `sync-push`, `sync-pull`)
* Импорт из других лаунчеров (`wilfred import --list/--detect/auto --dry-run/--overwrite`): Alfred, Raycast, PowerToys Run, Flow Launcher/Wox, Keypirinha, Listary, Ulauncher, Albert, KRunner, Rofi -- см. `docs/import.md`
* YAML-конфиг с валидацией, `Did you mean` хинтами и читаемыми ошибками
* Глобальный хоткей: `Ctrl+Alt+W` (Windows/Linux), `⌘+Alt+W` (macOS), настраиваемый
* Локальная история поиска (опционально; отключаемо или `wilfred history-clear`)
* Overlay UI + CLI (`search`, `launch`, `index`, `status`, backup/sync, `import`)

## Сборка

Требуется CMake 3.16+ и C++20 компилятор (MSVC, Apple Clang или GCC).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

На Linux X11 dev-заголовки включают оверлей (`libx11-dev`); в Wayland-сессиях WebKit-оверлей становится нативной layer-shell поверхностью при установке `libgtk-layer-shell-dev` (Sway/wlroots, KDE Plasma — GNOME не имеет layer-shell протокола, окно остаётся обычным toplevel).

## Запуск

```bash
# Фоновый демон: оверлей, индексер, хоткей
wilfred

# Однократный поиск по локальному индексу
wilfred search firefox

# Поиск и открытие топ-результата
wilfred launch firefox

# Индексация настроенных корней сейчас
wilfred index

# Статистика
wilfred status

# Импорт настроек из другого лаунчера
wilfred import --list
wilfred import --detect
wilfred import auto --dry-run
wilfred import alfred --from Alfred.alfredpreferences --dry-run
```

Пользовательский конфиг создаётся при первом запуске:

| Платформа | Конфиг | Данные индекса |
|---|---|---|
| Windows | `%APPDATA%\Wilfred\wilfred.yml` | `%LOCALAPPDATA%\Wilfred\` |
| macOS | `~/Library/Application Support/Wilfred/` | same |
| Linux | `~/.config/wilfred/wilfred.yml` | `~/.local/share/wilfred/` |
| BSD | `~/.config/wilfred/wilfred.yml` | `~/.local/share/wilfred/` |

См. `config/wilfred.default.yml` для полной схемы (индекс-руты, исключения, системные директории, ранкинг-веса, алиасы, хоткей, история, браузер).

## Миграция с другого лаунчера

`wilfred import` приносит хоткеи, кастомные веб-поиски (как `macros:` + `quicklinks:`), сниппеты, алиасы, тему и дефолтный поисковый шаблон в `wilfred.yml`. Мержинг по умолчанию; `--overwrite` заменяет конфликты, `--dry-run` превью, и `.pre-import.bak` бэкап пишется перед каждым реальным импортом.

| OS | Поддерживаемые источники |
|---|---|
| macOS | Alfred (`Alfred.alfredpreferences` bundle), Raycast (quicklinks JSON) |
| Windows | PowerToys Run, Flow Launcher, Wox, Keypirinha, Listary |
| Linux | Ulauncher, Albert, KRunner / KDE, Rofi |

См. `docs/import.md` (и `wilfred import --list`) для пер-лаунчер маппинга.

## Язык запросов (примеры)

| Запрос | Значение |
|---|---|
| `firefox` | Нечёткий поиск файла/приложения |
| `25 * 42` | Калькулятор |
| `https://example.com` | Открыть URL |
| `? кошки` / `g кошки` | Веб-поиск в браузере по умолчанию |
| `*.cpp in Projects` | Расширение + фильтр директории |
| `type:image name:logo` | Тип + имя файла |
| `content:widget` / `intext foo` | Поиск в индексированном содержимом файлов |
| `> notepad` | Команда / стиль запуска |
| `scope:home заметки` | Именованная группа директорий из конфига |
| `погода` / `погода Москва` | Мини-карта: локальный или городской прогноз |
| `speedtest` | Живой ping, download, upload (`speedtest again` перезапускает) |
| `время` `диск` `диск_использование` `ram` `cpu` | Часы, диски, память, процессор |
| `процесс chrome` / `top` | Живые процессы CPU, RAM, потоки (`kill <pid|name>` завершает) |
| `скриншот` / `скриншот window` / `скриншот region` | Захват полного экрана, окна или области в `Pictures/Wilfred` |
| `emoji улыбка` / `symbol евро` | Эмодзи и символы пикер (Enter копирует) |
| `100 usd в eur` / `fx 25 gbp jpy` | Конвертация валют |
| `1 ложка в г` / `1 чашка муки в г` | Кулинарный объём↔масса (вода по умолчанию, опц. ингредиент) |
| `#ff5500` / `rgb(255, 85, 0)` / `color coral` | Конвертация цвета (hex / rgb / hsl) |
| `15pm est в pst` / `now in tokyo` / `tz london` | Часовые пояса |
| `сегодня + 7 дней` / `2024-01-31 + 1 месяц` | Дата-арифметика |
| `uuid` / `base64 привет` / `sha256 abc` / `lorem 12` / `json {"a":1}` | Dev-утилиты (Enter копирует) |
| `hex 255` / `dec 0xff` / `0b1010` / `bit and 12 10` | Численные базы + битовые инструменты |
| `regex foo.* foobar` / `urlencode a b` / `jwt <token>` | Regex-тестер, URL-кодек, JWT-декодер |
| `timer 10m` / `pomodoro` / `stopwatch start` | Обратный отсчёт, пресеты Pomodoro, секундомер |
| `note купить молоко` / `notes` / `todo отправить` / `todos` | Быстрые заметки + задачи (`todo done 1`) |
| `media play` / `media next` / `media mute` | Нативные медиа-клавиши + громкость (Win media keys; mac HID + Music/Spotify/VLC; Linux native MPRIS D-Bus + wpctl/pactl/amixer, playerctl опционально) |
| `ping example.com` / `dns example.com` / `myip` | Ping, DNS lookup, публичный IP |
| `large 10` / `dupes` | Самые большие файлы + кандидаты дублей из индекса |
| `transcribe разговор.mp3` / `dictate 10` | Speech-to-text для mp3/mp4 аудио, микрофон диктовка (`transcription.engine`: whisper CLI или встроенный spoke engine) |
| `convert song.wav to mp3` / `convert photo.bmp to png` | Конвертация файлов (native PCM + BMP/PNG/PPM/TGA, ffmpeg для сжатых) |
| `bgremove photo.png` / `bgremove photo.png 40 #ffffff` | Удаление фона в прозрачный PNG (авто-углы или явный цвет) |
| `окна chrome` / `минимизировать spotify` | Список, переключение, минимизация, максимизация, закрытие, snap окон |
| `layout save work` / `layout work` | Сохранение и восстановление лэйаутов окон |
| `define resilient` / `thesaurus happy` | Офлайн определение + синонимы (Enter копирует) |
| `pins` / `pin firefox` / `unpin firefox` | Закреплённые избранное (буст через `ranking.pinned`) |
| `event add Team sync | завтра 10am` | Создание календарного события (пишет `.ics`) |
| `contact add Jane Doe jane@x.com 555-0100` | Создание контакта (пишет `.vcf`) |
| `rename ./photos photo-{n}.jpg` | Массовое переименование с `{n}` `{name}` `{ext}` |
| `template python foo in ./dir` | Новый файл из шаблона (md/python/cpp/html/json/gitignore) |
| `clips image` / `clips path` | История буфера обмена, отфильтрованная по типу (text/url/email/path/code/ip/image) |
| `workflow review` / `run review` | Именованные многошаговые workflows (`workflows` lists) |
| `ql docs привет` / `ticket:ABC-123` | Параметризованные quicklinks (`{1}`/`{*}`/`{query}`) |
| `lock` / `sleep` / `shutdown` / `restart` / `logout` / `empty trash` | Системные команды |
| `wifi` / `wifi off` / `bluetooth toggle` | Wi-Fi + Bluetooth статус и on/off (Enter применяет) |
| `volume 50` / `volume mute` / `brightness 70` | Уровень громкости/мьют + яркость экрана (Enter применяет) |
| `settings wifi` / `settings sound` / `settings` | Открыть страницы настроек ОС (wifi/network/bluetooth/sound/display/...) |
| `setup` / `config validate` / `config open` | Статус первого запуска + edit/validate wilfred.yml (`wilfred setup`, `config-validate`) |
| `update` / `upgrade` | Статус новой версии из startup-check (Enter копирует ссылку, `wilfred update` устанавливает) |
| `settings edit search.max_results 40` / `settings get hotkey.key` | Обновление/чтение одной настройки in-place, валидированное (`wilfred config-set/get`) |
| `plugins` / `plugins approve demo` | Список плагинов + trust approvals (`wilfred plugin list/install/approve`) |
| `layout tile grid` / `tile halves` / `layout work` | Тайлинг открытых окон или применение сохранённого лэйаута (`wilfred tile`, `layouts.auto_apply` on monitor change) |
| `winget firefox` / `brew switch` / `apt vlc` | Package search -- Enter устанавливает, `Tab` копирует команду (`packages` lists managers) |
| `clip` / `clips` | Буфер обмена и недавние клипы |
| `!yt кошки` / `gh wilfred` | Search macros (`macros` lists them) |

## Архитектура

Platform I/O живёт за `wilfred/platform/native.hpp`. Core модули:

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
* **Mobile is experimental (as of v23.0.5).** The Android (`.apk`) and iOS (simulator `.app`) builds install and run but are not daily-driver ready pending further testing and development -- deploy desktop first (see `docs/android.md`, `docs/ios.md`).