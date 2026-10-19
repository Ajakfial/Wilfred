# (ESTA README FUE HECHA POR CURSOR AI, TODO LO DEMÁS, INCLUYENDO EL CÓDIGO, FUE ESCRITO POR MANO HUMANA.)

# Wilfred

**Sitio web:** https://ajakfial.github.io/Wilfred/

Wilfred es un motor de búsqueda y lanzador de aplicaciones de escritorio rápido, ligero y multiplataforma. Está diseñado como una capa profunda de búsqueda del SO: archivos, carpetas, aplicaciones, integración con el navegador, calculadora y consultas filtradas desde una única barra de búsqueda invocable.

**RÁPIDO. PROFUNDO. MODULAR. BAJO RECURSO. MULTIPLATAFORMA. EXTENSIBLE.**

## Características

* Índice incremental, persistente de nombres de archivo/rutas/metadatos con write-ahead log
* Observadores de sistema de archivos (ReadDirectoryChangesW, FSEvents, inotify) más respaldo de reescaneo
* Búsqueda difusa, acrónimos, ranking por token/ruta, señales de recencia y frecuencia
* Seguridad ante errores tipográficos: Damerau (con transposición) *near-miss* en nombres, tokens y rutas, más corrección `¿Quiso decir?` (comandos, historial, nombres de archivo)
* Autocompletado en línea: completación fantasma (`→` para aceptar) + top-N candidatos del historial, comandos e índice
* Ranking consciente del contexto (carpetas recientes, tipos de archivo, hora del día, consultas de sesión)
* Portapapeles como fuente secundaria (coincidencia de texto, rutas copiadas, historial de clips)
* Gestor persistente de portapapeles (`clips [consulta]`, fijar, `clips clear`, `clips url|email|path|code|ip`)
* Indexación de contenido para código fuente, configuración y documentos de texto
* Extracción real de texto de documentos (PDF, Office, RTF, HTML) al índice de contenido
* Búsqueda semántica opcional: embeddings locales (servidor/modelo llama.cpp o hash integrado) + índice vectorial HNSW (`vectors.bin` junto a WAL/snapshot) fusionado con coincidencia suave de trigramas (`providers.semantic`, `embedding.*`)
* Asistente de IA opcional (`ai ...`/`ask ...`, `ai see ...` consciente de pantalla) con sus propias claves: OpenAI, Anthropic, Gemini, Groq
* Snippets con carpetas, placeholders `{date}`/`{clipboard}`/`{query}`, y abreviaturas globales opcionales en cualquier app
* Notas rápidas + tareas (`note ...`, `todo ...`, persistentes en el directorio de datos)
* Pomodoro / temporizadores cuenta atrás + cronómetro (`timer`, `pomodoro`, `stopwatch`)
* Vistas previas ricas de archivos (documentos, CSV, imágenes, carpetas) en el overlay (`F3`) más `wilfred preview`
* Calendario (`.ics`), contactos (`.vcf`) y notas (Markdown) + OCR opcional vía `tesseract` (`sources.*`)
* Overlay multiplataforma (Windows WebView2 topmost, macOS panel flotante, Linux WebKitGTK con fallback automático a canvas X11, layer-shell anclado en compositores Wayland, todos above/skip-taskbar) con popover de acciones `Ctrl`/`⌘`+`K` y atajos coincidentes en ambos backends Linux
* Marcadores, historial y pestañas abiertas del navegador (`bm`, respaldado por provider)
* Enumeración instantánea de disco completo NTFS via journal USN en Windows
* Federación oficial de índice del SO (`os_search:`): Windows Search, Spotlight, Tracker/Baloo, `locate` rellenan huecos que el índice de Wilfred no cubre (`os <consulta>` lo fuerza, duplicados eliminados)
* Panel de vista previa en el overlay (`F3`) más `wilfred preview`, y acciones encadenadas de resultados (`open+copy_path`) + flujos de trabajo multi-paso nombrados (`workflows:`)
* Mini-resultados para clima, hora, disco, RAM, CPU, procesos, batería, ventanas, temporizadores, notas, medios, red, layouts y más
* Gestión de ventanas (`windows`, `minimize`/`maximize`/`close window`, snap) más layouts de ventana guardados (`layout save`, `layout`)
* IA consciente de pantalla (`ai see ...` envía captura como entrada de visión a su proveedor)
* Las notas rápidas ahora son archivos Markdown reales, buscables tanto como tarjetas como a través de búsqueda de notas
* Automatización system-wide: hotkeys globales extra (`hotkeys:`), triggers CLI `wilfred workflow` / `wilfred exec`
* Macros de búsqueda (`!yt`, `gh`, `wiki`, más plantillas personalizadas en config) + enlaces rápidos parametrizados (`quicklinks:` con `{1}`/`{*}`/`{query}`)
* Acciones de contexto extra por app (`app_actions:`) sobre las acciones de archivo integradas
* Buscador de archivos grandes + candidatos duplicados del índice (`large`, `dupes`)
* Conversión de archivos (`convert song.wav to mp3`, `convert photo.bmp to png`: nativo WAV/RAW + BMP/PNG/PPM/TGA, ffmpeg para mp3/ogg/flac/jpg/webp) + eliminación de fondo (`bgremove photo.png` a PNG transparente, sin modelo necesario)
* Mata-procesos (`kill <pid|name>`), teclas multimedia nativas (`media play/next/mute/vol` -- sin helpers que instalar), herramientas de red (`ping`, `dns`, `myip`), voz-a-texto (`transcribe` vía whisper CLI o motor spoke integrado)
* Filtros componibles (`*.cpp in Projects`, `type:image`, `size:>10mb`, ámbitos nombrados)
* Descubrimiento de aplicaciones (Menú Inicio / bundles `.app` / archivos `.desktop`)
* Acciones ricas de archivo (variantes de copiar ruta, hash SHA-256, zip, terminal/editor aquí, nuevo archivo/carpeta, Abrir con)
* Detección de navegador por defecto, apertura de URL, y fallback de búsqueda web
* Parser seguro de expresiones (aritmética, funciones, conversión unidad/moneda/color/fecha, uuid/base64/sha256/lorem/json, hex/dec/bin/oct, bits, regex/url/jwt)
* Plugins nativos y stdio, plus backends de búsqueda extra vía providers
* Calculadora más unidades, monedas, colores, fecha y pequeñas utilidades de dev
* Comandos de sistema desde el overlay (`lock`, `sleep`, `shutdown`, `restart`, `logout`, vaciar papelera)
* Conmutadores de sistema (`wifi on/off`, `bluetooth toggle`, `volume 50/mute`, `brightness 70`) + deep-links a ajustes (`settings wifi/bluetooth/sound/display/...`) -- backends reales de SO en Windows/macOS/Linux, errores honestos "no soportado" en mobile/BSD
* Búsqueda de gestores de paquetes (`winget firefox`, `brew switch`, `apt vlc` -- Enter instala, `Tab` copia el comando; `packages` lista gestores detectados)
* Ajustes + setup (`settings config open/validate`, `settings edit <key> <value>`, `setup`, `wilfred setup --overwrite`, `wilfred config-get/set/reset`, `wilfred config-validate/open/path`) y backends de búsqueda remota opt-in (`remotes:` -- solo sus endpoints, off por defecto; por-fuente `headers:` + `max_results:`)
* Plugins con confianza al primer uso (`plugins` tarjetas de aprobación, `wilfred plugin list/install/approve/revoke`, índice `plugins.registry` opcional, instalaciones verificadas sha256)
* Mosaico de ventanas (`layout tile halves|thirds|grid|columns N|rows N|stack`, `wilfred tile …`) + auto-aplicación en cambio de monitor (`layouts.auto_apply`, `auto_layout`, `monitor_layouts`)
* App Android (UI Kotlin + núcleo C++ vía NDK → `.apk`): botón W flotante, barra de búsqueda, mismo índice/búsqueda/ranking -- ver `docs/android.md`
* App iOS (SwiftUI + mismo núcleo C++ → `.app` simulador): misma UI de búsqueda que Android (entrada por icono de app; iOS prohíbe botones overlay), índice solo en sandbox -- ver `docs/ios.md`
* Backup/restauración local y sync remoto opcional (`backup`, `restore`, `sync-push`, `sync-pull`)
* Importación desde otros launchers (`wilfred import --list/--detect/auto --dry-run/--overwrite`): Alfred, Raycast, PowerToys Run, Flow Launcher/Wox, Keypirinha, Listary, Ulauncher, Albert, KRunner, Rofi -- ver `docs/import.md`
* Configuración YAML con validación, pistas `¿Quiso decir?` y errores legibles
* Hotkey global: `Ctrl+Alt+W` (Windows/Linux), `⌘+Alt+W` (macOS), configurable
* Historial de búsqueda local (opcional; desactivable o `wilfred history-clear`)
* UI overlay + CLI (`search`, `launch`, `index`, `status`, backup/sync, `import`)

## Compilación

Requiere CMake 3.16+ y compilador C++20 (MSVC, Apple Clang, o GCC).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

En Linux, cabeceras de desarrollo X11 habilitan el overlay (`libx11-dev`); en sesiones Wayland el overlay WebKit se convierte adicionalmente en superficie nativa layer-shell cuando `libgtk-layer-shell-dev` está instalado (Sway/wlroots, KDE Plasma -- GNOME no tiene protocolo layer-shell, la ventana permanece un toplevel normal).

## Ejecución

```bash
# Demonio en segundo plano: overlay, indexer, hotkey
wilfred

# Búsqueda única contra el índice local
wilfred search firefox

# Busca y abre el mejor resultado
wilfred launch firefox

# Indexa roots configurados ahora
wilfred index

# Estadísticas
wilfred status

# Importa ajustes de otro launcher
wilfred import --list
wilfred import --detect
wilfred import auto --dry-run
wilfred import alfred --from Alfred.alfredpreferences --dry-run
```

La configuración de usuario se crea en la primera ejecución:

| Plataforma | Config | Datos de índice |
|---|---|---|
| Windows | `%APPDATA%\Wilfred\wilfred.yml` | `%LOCALAPPDATA%\Wilfred\` |
| macOS | `~/Library/Application Support/Wilfred/` | same |
| Linux | `~/.config/wilfred/wilfred.yml` | `~/.local/share/wilfred/` |
| BSD | `~/.config/wilfred/wilfred.yml` | `~/.local/share/wilfred/` |

Véase `config/wilfred.default.yml` para el esquema completo (raíces de índice, exclusiones, directorios de sistema, pesos de ranking, alias, hotkey, historial, navegador).

## Migración desde otro launcher

`wilfred import` trae hotkeys, búsquedas web personalizadas (como `macros:` + `quicklinks:`), snippets, alias, tema y la plantilla de búsqueda por defecto a `wilfred.yml`. La fusión es por defecto; `--overwrite` reemplaza conflictos, `--dry-run` previsualiza, y un backup `.pre-import.bak` se escribe antes de cada importación real.

| OS | Fuentes soportadas |
|---|---|
| macOS | Alfred (`Alfred.alfredpreferences` bundle), Raycast (quicklinks JSON) |
| Windows | PowerToys Run, Flow Launcher, Wox, Keypirinha, Listary |
| Linux | Ulauncher, Albert, KRunner / KDE, Rofi |

Véase `docs/import.md` (y `wilfred import --list`) para mapeo por launcher.

## Lenguaje de consultas (ejemplos)

| Consulta | Significado |
|---|---|
| `firefox` | Búsqueda difusa archivo/app |
| `25 * 42` | Calculadora |
| `https://example.com` | Abrir URL |
| `? gatos` / `g gatos` | Búsqueda web en navegador por defecto |
| `*.cpp in Projects` | Extensión + filtro directorio |
| `type:image name:logo` | Tipo + nombre archivo |
| `content:widget` / `intext foo` | Buscar contenido indexado |
| `> notepad` | Estilo comando / lanzamiento |
| `scope:home notas` | Grupo de directorios nombrado de la config |
| `tiempo` / `tiempo Madrid` | Mini tarjeta: local o pronóstico ciudad |
| `speedtest` | Ping, descarga y subida en vivo (`speedtest again` repite) |
| `hora` `disco` `discou` `ram` `cpu` | Reloj, discos, memoria, procesador |
| `proceso chrome` / `top` | Proceso vivo CPU, RAM, hilos (`kill <pid|name>` mata) |
| `captura` / `captura ventana` / `captura region` | Captura pantalla completa, ventana o región a `Pictures/Wilfred` |
| `emoji sonrisa` / `simbolo euro` | Selector emoji/símbolos (Enter copia) |
| `100 usd a eur` / `fx 25 gbp jpy` | Conversión de divisa |
| `1 cda a g` / `1 taza harina a g` | Volumen↔masa cocina (agua por defecto, ingrediente opcional) |
| `#ff5500` / `rgb(255, 85, 0)` / `color coral` | Conversión color (hex / rgb / hsl) |
| `3pm est a pst` / `now in tokyo` / `tz london` | Zonas horarias |
| `hoy + 7 dias` / `2024-01-31 + 1 mes` | Aritmética de fechas |
| `uuid` / `base64 hola` / `sha256 abc` / `lorem 12` / `json {"a":1}` | Utilidades dev (Enter copia) |
| `hex 255` / `dec 0xff` / `0b1010` / `bit and 12 10` | Bases numéricas + herramientas bit |
| `regex foo.* foobar` / `urlencode a b` / `jwt <token>` | Test regex, codec URL, JWT decode |
| `timer 10m` / `pomodoro` / `cronometro start` | Cuentas atrás, presets Pomodoro, cronómetro |
| `note comprar leche` / `notes` / `todo enviar` / `todos` | Notas rápidas + tareas (`todo done 1`) |
| `media play` / `media next` / `media mute` | Teclas multimedia nativas + volumen (Win media keys; mac HID + Music/Spotify/VLC; Linux MPRIS D-Bus nativo + wpctl/pactl/amixer, playerctl opcional) |
| `ping example.com` / `dns example.com` / `myip` | Ping, lookup DNS, IP pública |
| `large 10` / `dupes` | Archivos más grandes + candidatos duplicados del índice |
| `transcribe charla.mp3` / `dictate 10` | Voz-a-texto para audio mp3/mp4, dictado micro (`transcription.engine`: whisper CLI o spoke integrado) |
| `convert song.wav to mp3` / `convert photo.bmp to png` | Conversión archivo (PCM nativo + BMP/PNG/PPM/TGA, ffmpeg para comprimidos) |
| `bgremove photo.png` / `bgremove photo.png 40 #ffffff` | Eliminación fondo a PNG transparente (esquinas auto o color explícito) |
| `ventanas chrome` / `minimizar spotify` | Listar, cambiar, minimizar, maximizar, cerrar, snap ventanas |
| `layout save trabajo` / `layout trabajo` | Guardar y restaurar layouts de ventanas |
| `define resilient` / `thesaurus happy` | Definición offline + sinónimos (Enter copia) |
| `pins` / `pin firefox` / `unpin firefox` | Favoritos anclados (impulsados via `ranking.pinned`) |
| `event add Reunion equipo | mañana 10am` | Crear evento calendario (escribe `.ics`) |
| `contact add Jane Doe jane@x.com 555-0100` | Crear contacto (escribe `.vcf`) |
| `rename ./fotos foto-{n}.jpg` | Renombrado masivo con `{n}` `{name}` `{ext}` |
| `template python foo in ./dir` | Nuevo archivo desde plantilla (md/python/cpp/html/json/gitignore) |
| `clips image` / `clips path` | Historial portapapeles filtrado por tipo (text/url/email/path/code/ip/image) |
| `workflow revision` / `run revision` | Flujos multi-paso nombrados (`workflows` lista) |
| `ql docs hola` / `ticket:ABC-123` | Enlaces rápidos parametrizados (`{1}`/`{*}`/`{query}`) |
| `lock` / `sleep` / `shutdown` / `restart` / `logout` / `empty trash` | Comandos de sistema |
| `wifi` / `wifi off` / `bluetooth toggle` | Estado Wi-Fi/Bluetooth y on/off (Enter aplica) |
| `volume 50` / `volume mute` / `brightness 70` | Volumen nivel/mute + brillo pantalla (Enter aplica) |
| `settings wifi` / `settings sound` / `settings` | Abrir páginas de ajustes del SO (wifi/network/bluetooth/sound/display/...) |
| `setup` / `config validate` / `config open` | Estado primer arranque + editar/validar wilfred.yml (`wilfred setup`, `config-validate`) |
| `update` / `upgrade` | Estado nueva versión del check de arranque (Enter copia link, `wilfred update` instala) |
| `settings edit search.max_results 40` / `settings get hotkey.key` | Actualiza/lee un ajuste in-place, validado (`wilfred config-set/get`) |
| `plugins` / `plugins approve demo` | Lista plugins + aprobaciones de confianza (`wilfred plugin list/install/approve`) |
| `layout tile grid` / `tile halves` / `layout trabajo` | Mosaico ventanas o aplica layout guardado (`wilfred tile`, `layouts.auto_apply` en cambio monitor) |
| `winget firefox` / `brew switch` / `apt vlc` | Búsqueda paquetes -- Enter instala, `Tab` copia comando (`packages` lista gestores) |
| `clip` / `clips` | Portapapeles y clips recientes |
| `!yt gatos` / `gh wilfred` | Macros de búsqueda (`macros` lista) |

## Arquitectura

I/O de plataforma vive detrás de `wilfred/platform/native.hpp`. Módulos núcleo:

* `config` -- Parse YAML + validación schema
* `index` -- String-intern pool, records compactos, índices invertidos/trigrama, WAL
* `fs` -- Walker, clasificación, volúmenes, watcher
* `search` -- Fuzzy, pesos ranking, filtros, cache de consultas
* `query` -- Clasificación + intérprete
* `math` -- Calculadora recursivo-descendente
* `apps` / `browser` / `history` / `hotkey` / `ipc` / `ui` / `service`
* `plugin` -- ABI nativo `.dll`/`.so`/`.dylib` y plugins stdio, trust store, registry index
* `sync` -- Archivos backup locales y push/pull remoto opcional
* `import` -- Importador cross-OS de ajustes de launchers (hotkeys, búsquedas, snippets)
* `providers` -- Registry para backends de búsqueda adicionales (semántico, biblioteca navegador, calendario/contactos/notas, remotes, paquetes)

## Tests y Benchmarks

```bash
cmake --build build --config Release --target wilfred_tests wilfred_bench
./build/wilfred_tests          # o build/Release/wilfred_tests.exe
./build/wilfred_bench
```

Tests cubren CRUD índice, persistencia/recuperación, fuzzy/ranking/filtro, matemáticas, clasificación consultas, validación config, historial, rutas, IPC y descubrimiento apps.

## Rendimiento

Wilfred es un único binario C++20 sin librerías terceras externas -- todo (JSON, YAML, mmap wrapper, CRC32) está implementado en el árbol, así que no hay nada extra que instalar (el overlay Linux usa librerías X11/WebKit del sistema cuando están presentes, más gtk-layer-shell para anclaje Wayland nativo, con fallback canvas X11). La velocidad viene del diseño del índice (ver `docs/architecture.md`):

* Almacén compacto de records memory-mapped con pool de strings internado (sin duplicados de strings ruta/nombre en RAM)
* Índices trigrama + token para que consultas golpeen posting lists, no escaneos completos
* Write-ahead log con snapshots periódicos y recuperación ante fallos -- escrituras son appends, lecturas nunca bloquean en persistencia
* Actualizaciones incrementales debounced de watchers de FS en lugar de reescaneos
* Conjuntos de resultados acotados (top-40) con timeouts por consulta en plugins

`wilfred_bench` (`benches/bench_main.cpp`) mide los hot paths en datasets sintéticos de 1K-100K nombres de archivo (10 iteraciones cronometradas + 1 warmup, 5 para la inserción 100K) y reporta Min / Median / Mean / P95 / Max milisegundos:

| Benchmark | Qué mide |
|---|---|
| String pool: intern 100K | Throughput interning (eficiencia memoria índice) |
| Tokenizer: tokenize 100K | Tokenización nombres archivo |
| Index insert: 1K / 10K / 100K | Construcción masiva índice |
| Index query: 10K / 100K records | Lookup posting-lists |
| Fuzzy search: 10K targets | Throughput scoring Damerau-consciente |
| Ranking: 10K scored hits | Weighted scoring + top-40 selection |
| Filter chain: 10K records | Evaluación filtro extensión/tipo |
| Math eval: 1K expressions | Calculator parse + eval |
| Snapshot save/load: 10K | Round-trip persistencia |
| Memory reporting: 100K index | Contabilidad footprint índice |

Ejecute en su hardware antes de citar números -- resultados dependen de CPU, disco y forma del dataset:

```bash
cmake --build build --config Release --target wilfred_bench
./build/wilfred_bench          # o build/Release/wilfred_bench.exe
```

## Empaquetado

Un push de tag `v*.*.*` lanza `.github/workflows/release.yml`, que construye binarios Release y adjunta un archivo por plataforma al GitHub Release (ver `docs/building.md#packaging-what-ci-does`):

| Job | Artefactos |
|---|---|
| `windows-x64` | `.zip` portable, `.msi` firmado (WiX v3, per-user, sin UAC), Chocolatey `.nupkg` (portable embebido, sin descarga al instalar) |
| `linux-x64` | `.tar.gz` portable |
| `freebsd-x64` | `.tar.gz` portable (FreeBSD release job; otros BSDs compilan desde fuente -- ver `docs/bsd.md`) |
| `macos-arm64` | `.tar.gz` portable |
| `android` | `.apk` + `.aab` -- firmados si existen secrets keystore, si no `-unsigned` sufijo (ver `docs/android.md`) |
| `ios` | `.zip` simulador sin firmar (ver `docs/ios.md`) |

Los archivos desktop incluyen el binario con `ui/overlay/`, `README.md` y `config/wilfred.default.yml`. Los binarios Windows llevan siempre el sello de editor `Wilfred Open Contributors` via VERSIONINFO; la confianza Authenticode viene de PFX o Azure Trusted Signing secrets cuando están configurados (ver `docs/signing.md`, `docs/installer.md`).

## Despliegue en producción

* **Ejecute el daemon, no one-shots.** `wilfred daemon` (el simple `wilfred`) es el proceso longevo: indexer + watcher + hotkey + overlay + servidor IPC. Ejecútelo como elemento de inicio / entrada autostart (`wilfred autostart enable` registra este binario en Windows/macOS/Linux/BSD); el icono de bandeja (Windows) lo termina. Los subcomandos CLI (`search`, `launch`, `status`, …) hablan con el daemon corriendo via socket IPC local cuando está disponible (ver `docs/cli.md`, `docs/ipc-and-api.md`).
* **La config es código.** La primera ejecución escribe un `wilfred.yml` validado (ubicaciones tabla arriba); claves inválidas fallan con errores legibles y pistas `¿Quiso decir?`, nunca defaults silenciosos. Para flotas, distribuya un `wilfred.yml` base (roots, exclusiones, pesos, alias -- schema completo en `docs/configuration.md` y `config/wilfred.default.yml`) y deje que los usuarios sobrescriban localmente.
* **Datos y DR.** Índice, historial, clips, notas y logs viven en el directorio de datos por-SO (tabla arriba). `backup` / `restore` mueven el estado de una máquina; `sync-push` / `sync-pull` lo replican (ver `docs/sync-and-backup.md`). Pruebe restauraciones antes de necesitarlas.
* **Observe.** `wilfred status` reporta salud del índice; claves `logging.*` controlan verbosidad/retención de logs; la API HTTP local opcional (`api.*`, autenticada) expone `/search`, `/status`, `/index`, `/launch` y endpoints backup/sync para dashboards y automatización.
* **Postura de privacidad.** Local-first: sin cuenta, sin telemetría, sin llamadas de red a menos que las habilite (clima/IA/sync). Desactive o limite historial via `history.*` donde la política lo requiera; proveedores IA necesitan sus propias API keys (`ai.*`, off por defecto).
* **Móvil es experimental (desde v23.0.5).** Las builds Android (`.apk`) e iOS (simulador `.app`) instalan y corren pero no están listos para daily-driver pendiente más pruebas y desarrollo -- despliegue desktop primero (ver `docs/android.md`, `docs/ios.md`).