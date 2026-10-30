# (ESTE README FOI FEITO PELO CURSOR AI, TODO O RESTO, INCLUINDO CÓDIGO, FOI ESCRITO POR MÃO HUMANA.)

# Wilfred

**Site:** https://ajakfial.github.io/Wilfred/

Wilfred é um motor de busca e lançador de aplicações de desktop rápido, leve e multiplataforma. É projetado como uma camada profunda de busca do SO: arquivos, pastas, aplicações, integração com navegador, calculadora e consultas filtradas a partir de uma única barra de busca invocável.

**RÁPIDO. PROFUNDO. MODULAR. BAIXO RECURSO. MULTIPLATAFORMA. EXTENSÍVEL.**

## Funcionalidades

* Índice incremental, persistente de nomes de arquivo/caminhos/metadados com log write-ahead
* Observadores de sistema de arquivos (ReadDirectoryChangesW, FSEvents, inotify) mais fallback de reescaneamento
* Busca fuzzy, acrônimos, ranking por token/caminho, sinais de recência e frequência
* Segurança contra erros de digitação: Damerau (consciente de transposição) near-miss em nomes, tokens e caminhos, mais correção `Você quis dizer` (comandos, histórico, nomes de arquivo)
* Autocompletar inline: completão fantasma (`→` para aceitar) + top-N candidatos do histórico, comandos e índice
* Ranking consciente de contexto (pastas recentes, tipos de arquivo, hora do dia, consultas da sessão)
* Clipboard como fonte secundária (match de texto, caminhos copiados, histórico de clips)
* Gerenciador de clipboard persistente (`clips [consulta]`, fixar, `clips clear`, `clips url|email|path|code|ip`)
* Indexação de conteúdo para código fonte, config e documentos de texto
* Extração real de texto de documentos (PDF, Office, RTF, HTML) no índice de conteúdo
* Busca semântica opcional: embeddings locais (servidor/modelo llama.cpp ou hash embutido) + índice vetorial HNSW (`vectors.bin` ao lado de WAL/snapshot) mesclado com soft-match de trigramas (`providers.semantic`, `embedding.*`)
* Assistente de IA opcional (`ai ...`/`ask ...`, `ai see ...` consciente de tela) com suas próprias chaves: OpenAI, Anthropic, Gemini, Groq
* Snippets com pastas, placeholders `{date}`/`{clipboard}`/`{query}`, e abreviações globais opcionais em qualquer app
* Notas rápidas + todos (`note ...`, `todo ...`, persistentes no diretório de dados)
* Pomodoro / timers countdown + cronômetro (`timer`, `pomodoro`, `stopwatch`)
* Previews ricos de arquivos (documentos, CSV, imagens, pastas) no overlay (`F3`) mais `wilfred preview`
* Calendário (`.ics`), contatos (`.vcf`) e notas (Markdown) fontes + OCR opcional via `tesseract` (`sources.*`)
* Overlay multiplataforma (Windows WebView2 topmost, macOS painel flutuante, Linux WebKitGTK com fallback automático canvas X11, layer-shell ancorado em compositores Wayland, todos above/skip-taskbar) com popover ações `Ctrl`/`⌘`+`K` e atalhos correspondentes em ambos backends Linux
* Favoritos, histórico e abas abertas do navegador (`bm`, provider-backed)
* Enumeração instantânea de disco NTFS completo via journal USN no Windows
* Federação oficial de índice do SO (`os_search:`): Windows Search, Spotlight, Tracker/Baloo, `locate` preenchem lacunas que o índice do Wilfred perde (`os <consulta>` força, duplicatas removidas)
* Painel de preview no overlay (`F3`) mais `wilfred preview`, e ações encadeadas de resultados (`open+copy_path`) + workflows multi-passo nomeados (`workflows:`)
* Mini-resultados para clima, hora, disco, RAM, CPU, processos, bateria, janelas, timers, notas, mídia, rede, layouts, e mais
* Gerenciamento de janelas (`windows`, `minimize`/`maximize`/`close window`, snap) + layouts de janela salvos (`layout save`, `layout`)
* IA consciente de tela (`ai see ...` envia screenshot como entrada de visão para seu provedor)
* Notas rápidas agora vivem como arquivos Markdown reais, buscáveis tanto como cards quanto via busca de notas
* Automação system-wide: hotkeys globais extras (`hotkeys:`), triggers CLI `wilfred workflow` / `wilfred exec`
* Macros de busca (`!yt`, `gh`, `wiki`, mais templates personalizados no config) + quicklinks parametrizados (`quicklinks:` com `{1}`/`{*}`/`{query}`)
* Ações de contexto extras por app (`app_actions:`) sobre as ações de arquivo integradas
* Buscador de arquivos grandes + candidatos duplicados do índice (`large`, `dupes`)
* Conversão de arquivos (`convert song.wav to mp3`, `convert photo.bmp to png`: nativo WAV/RAW + BMP/PNG/PPM/TGA, ffmpeg para mp3/ogg/flac/jpg/webp) + remoção de fundo (`bgremove photo.png` para PNG transparente, sem modelo necessário)
* Mata-processos (`kill <pid|name>`), teclas de mídia nativas (`media play/next/mute/vol` -- sem helpers para instalar), ferramentas de rede (`ping`, `dns`, `myip`), fala-para-texto (`transcribe` via whisper CLI ou engine spoke embutido)
* Filtros componíveis (`*.cpp in Projects`, `type:image`, `size:>10mb`, escopos nomeados)
* Descoberta de aplicações (Menu Iniciar / bundles `.app` / arquivos `.desktop`)
* Ações ricas de arquivo (variantes copiar caminho, hash SHA-256, zip, terminal/editor aqui, novo arquivo/pasta, Abrir com)
* Detecção de navegador padrão, abertura de URL, e fallback de busca web
* Parser seguro de expressões (aritmética, funções, conversão unidade/moeda/cor/data, uuid/base64/sha256/lorem/json, hex/dec/bin/oct, bits, regex/url/jwt)
* Plugins nativos e stdio, mais backends de busca extras via providers
* Calculadora mais unidades, moedas, cores, data, e pequenos utilitários dev
* Comandos de sistema do overlay (`lock`, `sleep`, `shutdown`, `restart`, `logout`, esvaziar lixeira)
* Toggles de sistema (`wifi on/off`, `bluetooth toggle`, `volume 50/mute`, `brightness 70`) + deep-links de config (`settings wifi/bluetooth/sound/display/...`) -- backends reais de SO no Windows/macOS/Linux, erros honestos "não suportado" em mobile/BSD
* Busca de gerenciadores de pacotes (`winget firefox`, `brew switch`, `apt vlc` -- Enter instala, `Tab` copia comando; `packages` lista gestores detectados)
* Config + setup (`settings config open/validate`, `settings edit <key> <value>`, `setup`, `wilfred setup --overwrite`, `wilfred config-get/set/reset`, `wilfred config-validate/open/path`) e backends de busca remota opt-in (`remotes:` -- seus endpoints apenas, off por padrão; por-fonte `headers:` + `max_results:`)
* Plugins com confiança no primeiro uso (`plugins` cards de aprovação, `wilfred plugin list/install/approve/revoke`, índice `plugins.registry` opcional, instalações verificadas sha256)
* Tiling de janelas (`layout tile halves|thirds|grid|columns N|rows N|stack`, `wilfred tile …`) + auto-aplicação em mudança de monitor (`layouts.auto_apply`, `auto_layout`, `monitor_layouts`)
* App Android (UI Kotlin + núcleo C++ via NDK → `.apk`): botão W flutuante, barra de busca, mesmo índice/busca/ranking -- veja `docs/android.md`
* App iOS (SwiftUI + mesmo núcleo C++ → `.app` simulador): mesma UI de busca que Android (entrada por ícone do app; iOS proíbe botões overlay), índice apenas sandbox -- veja `docs/ios.md`
* Backup/restauração local e sync remoto opcional (`backup`, `restore`, `sync-push`, `sync-pull`)
* Import de outros launchers (`wilfred import --list/--detect/auto --dry-run/--overwrite`): Alfred, Raycast, PowerToys Run, Flow Launcher/Wox, Keypirinha, Listary, Ulauncher, Albert, KRunner, Rofi -- veja `docs/import.md`
* Config YAML com validação, dicas `Você quis dizer` e erros legíveis
* Hotkey global: `Ctrl+Alt+W` (Windows/Linux), `⌘+Alt+W` (macOS), configurável
* Histórico de busca local (opcional; desativável ou `wilfred history-clear`)
* UI overlay + CLI (`search`, `launch`, `index`, `status`, backup/sync, `import`)

## Build

Requer CMake 3.16+ e compilador C++20 (MSVC, Apple Clang, ou GCC).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

No Linux, headers de desenvolvimento X11 habilitam o overlay (`libx11-dev`); em sessões Wayland, o overlay WebKit adicionalmente se torna uma superfície layer-shell nativa quando `libgtk-layer-shell-dev` está instalado (Sway/wlroots, KDE Plasma -- GNOME não tem protocolo layer-shell, onde a janela fica um toplevel simples).

## Execução

```bash
# Daemon em background: overlay, indexer, hotkey
wilfred

# Busca única contra o índice local
wilfred search firefox

# Busca e abre o melhor resultado
wilfred launch firefox

# Indexa roots configurados agora
wilfred index

# Estatísticas
wilfred status

# Importa configurações de outro launcher
wilfred import --list
wilfred import --detect
wilfred import auto --dry-run
wilfred import alfred --from Alfred.alfredpreferences --dry-run
```

Config do usuário é criada na primeira execução:

| Plataforma | Config | Dados do índice |
|---|---|---|
| Windows | `%APPDATA%\Wilfred\wilfred.yml` | `%LOCALAPPDATA%\Wilfred\` |
| macOS | `~/Library/Application Support/Wilfred/` | same |
| Linux | `~/.config/wilfred/wilfred.yml` | `~/.local/share/wilfred/` |
| BSD | `~/.config/wilfred/wilfred.yml` | `~/.local/share/wilfred/` |

Veja `config/wilfred.default.yml` para o schema completo (raízes de índice, exclusões, diretórios de sistema, pesos de ranking, aliases, hotkey, histórico, navegador).

## Migrando de outro launcher

`wilfred import` traz hotkeys, buscas web personalizadas (como `macros:` + `quicklinks:`), snippets, aliases, tema e o template de busca padrão para `wilfred.yml`. Merge é padrão; `--overwrite` substitui conflitos, `--dry-run` prévia, e um backup `.pre-import.bak` é escrito antes de cada importação real.

| OS | Fontes suportadas |
|---|---|
| macOS | Alfred (`Alfred.alfredpreferences` bundle), Raycast (quicklinks JSON) |
| Windows | PowerToys Run, Flow Launcher, Wox, Keypirinha, Listary |
| Linux | Ulauncher, Albert, KRunner / KDE, Rofi |

Veja `docs/import.md` (e `wilfred import --list`) para mapeamento por launcher.

## Linguagem de consultas (exemplos)

| Consulta | Significado |
|---|---|
| `firefox` | Busca fuzzy arquivo/app |
| `25 * 42` | Calculadora |
| `https://example.com` | Abrir URL |
| `? gatos` / `g gatos` | Busca web no navegador padrão |
| `*.cpp in Projects` | Extensão + filtro diretório |
| `type:image name:logo` | Tipo + nome arquivo |
| `content:widget` / `intext foo` | Buscar conteúdo indexado |
| `> notepad` | Estilo comando / lançamento |
| `scope:home notas` | Grupo de diretório nomeado da config |
| `tempo` / `tempo São Paulo` | Mini card: local ou previsão da cidade |
| `speedtest` | Ping, download e upload ao vivo (`speedtest again` repete) |
| `hora` `disco` `discou` `ram` `cpu` | Relógio, discos, memória, processador |
| `processo chrome` / `top` | Processo CPU, RAM, threads ao vivo (`kill <pid|name>` mata) |
| `captura` / `captura janela` / `captura regiao` | Captura tela cheia, janela ou região para `Pictures/Wilfred` |
| `emoji sorriso` / `simbolo euro` | Seletor emoji/símbolos (Enter copia) |
| `100 usd para eur` / `fx 25 gbp jpy` | Conversão de moeda |
| `1 colher a g` / `1 xicara farinha a g` | Volume↔massa culinária (água padrão, ingrediente opcional) |
| `#ff5500` / `rgb(255, 85, 0)` / `cor coral` | Conversão de cor (hex / rgb / hsl) |
| `15h est para pst` / `now in tokyo` / `tz londres` | Fusos horários |
| `hoje + 7 dias` / `2024-01-31 + 1 mes` | Matemática de datas |
| `uuid` / `base64 oi` / `sha256 abc` / `lorem 12` / `json {"a":1}` | Utilitários dev (Enter copia) |
| `hex 255` / `dec 0xff` / `0b1010` / `bit and 12 10` | Bases numéricas + ferramentas bit |
| `regex foo.* foobar` / `urlencode a b` / `jwt <token>` | Testador regex, codec URL, JWT decode |
| `timer 10m` / `pomodoro` / `cronometro start` | Countdowns, presets Pomodoro, cronômetro |
| `note comprar leite` / `notes` / `todo enviar` / `todos` | Notas rápidas + tarefas (`todo done 1`) |
| `media play` / `media next` / `media mute` | Teclas mídia nativas + volume (Win media keys; mac HID + Music/Spotify/VLC; Linux MPRIS D-BUS nativo + wpctl/pactl/amixer, playerctl opcional) |
| `ping example.com` / `dns example.com` / `meuip` | Ping, DNS lookup, IP público |
| `large 10` / `dupes` | Maiores arquivos + candidatos duplicados do índice |
| `transcribe fala.mp3` / `ditar 10` | Voz-para-texto para áudio mp3/mp4, ditado micro (`transcription.engine`: whisper CLI ou spoke embutido) |
| `convert song.wav to mp3` / `convert photo.bmp to png` | Conversão de arquivo (PCM nativo + BMP/PNG/PPM/TGA, ffmpeg para comprimidos) |
| `bgremove photo.png` / `bgremove photo.png 40 #ffffff` | Remoção de fundo para PNG transparente (cantos auto ou cor explícita) |
| `janelas chrome` / `minimizar spotify` | Listar, alternar, minimizar, maximizar, fechar, snap janelas |
| `layout save trabalho` / `layout trabalho` | Salvar e restaurar layouts de janelas |
| `define resilient` / `thesaurus happy` | Definição offline + sinônimos (Enter copia) |
| `pins` / `pin firefox` / `unpin firefox` | Favoritos fixados (impulsionados via `ranking.pinned`) |
| `event add Reuniao equipe | amanha 10am` | Criar evento calendário (escreve `.ics`) |
| `contact add Jane Doe jane@x.com 555-0100` | Criar contato (escreve `.vcf`) |
| `rename ./fotos foto-{n}.jpg` | Renomeação em massa com `{n}` `{name}` `{ext}` |
| `template python foo in ./dir` | Novo arquivo de template (md/python/cpp/html/json/gitignore) |
| `clips image` / `clips path` | Histórico clipboard filtrado por tipo (text/url/email/path/code/ip/image) |
| `workflow revisao` / `run revisao` | Workflows multi-step nomeados (`workflows` lista) |
| `ql docs oi` / `ticket:ABC-123` | Quicklinks parametrizados (`{1}`/`{*}`/`{query}`) |
| `lock` / `sleep` / `shutdown` / `restart` / `logout` / `empty trash` | Comandos de sistema |
| `wifi` / `wifi off` / `bluetooth toggle` | Status Wi-Fi + Bluetooth e on/off (Enter aplica) |
| `volume 50` / `volume mute` / `brilho 70` | Volume nível/mute + brilho tela (Enter aplica) |
| `settings wifi` / `settings sound` / `settings` | Abre páginas de config do SO (wifi/network/bluetooth/sound/display/...) |
| `setup` / `config validate` / `config open` | Status primeiro boot + edita/valida wilfred.yml (`wilfred setup`, `config-validate`) |
| `update` / `upgrade` | Status nova versão do check de boot (Enter copia link, `wilfred update` instala) |
| `settings edit search.max_results 40` / `settings get hotkey.key` | Atualiza/lê uma config no lugar, validado (`wilfred config-set/get`) |
| `plugins` / `plugins approve demo` | Lista plugins + aprovações de confiança (`wilfred plugin list/install/approve`) |
| `layout tile grid` / `tile halves` / `layout trabalho` | Tile janelas ou aplica layout salvo (`wilfred tile`, `layouts.auto_apply` em mudança monitor) |
| `winget firefox` / `brew switch` / `apt vlc` | Busca pacotes -- Enter instala, `Tab` copia comando (`packages` lista gestores) |
| `clip` / `clips` | Clipboard e clips recentes |
| `!yt gatos` / `gh wilfred` | Macros de busca (`macros` lista) |

## Arquitetura

I/O de plataforma vive atrás de `wilfred/platform/native.hpp`. Módulos core:

* `config` -- Parse YAML + validação schema
* `index` -- String-intern pool, records compactos, índices invertidos/trigrama, WAL
* `fs` -- Walker, classificação, volumes, watcher
* `search` -- Fuzzy, pesos ranking, filtros, cache de query
* `query` -- Classificação + interpretador
* `math` -- Calculadora recursive-descent
* `apps` / `browser` / `history` / `hotkey` / `ipc` / `ui` / `service`
* `plugin` -- ABI nativo `.dll`/`.so`/`.dylib` e plugins stdio, trust store, registry index
* `sync` -- Archives de backup locais e push/pull remoto opcional
* `import` -- Importador cross-OS de settings de launchers (hotkeys, buscas, snippets)
* `providers` -- Registry para backends de busca adicionais (semântico, biblioteca navegador, calendário/contatos/notas, remotes, pacotes)

## Testes e Benchmarks

```bash
cmake --build build --config Release --target wilfred_tests wilfred_bench
./build/wilfred_tests          # ou build/Release/wilfred_tests.exe
./build/wilfred_bench
```

Testes cobrem CRUD índice, persistência/recuperação, fuzzy/ranking/filtro, matemática, classificação query, validação config, histórico, paths, IPC e descoberta apps.

## Performance

Wilfred é um único binário C++20 sem bibliotecas terceiras externas -- tudo (JSON, YAML, mmap wrapper, CRC32) é implementado na árvore, então não há nada extra para instalar (o overlay Linux usa libs X11/WebKit do sistema quando presentes, mais gtk-layer-shell para ancoragem Wayland nativa, com fallback canvas X11). A velocidade vem do design do índice (veja `docs/architecture.md`):

* Armazenamento compacto de records memory-mapped com pool de string-intern (sem strings duplicadas de caminho/nome na RAM)
* Índices trigram + token para que queries atinjam posting lists, não scans completos
* Write-ahead log com snapshots periódicos e recuperação de crash -- escritas são appends, leituras nunca bloqueiam em persistência
* Atualizações incrementais, debounced de filesystem-watcher em vez de rescans
* Conjuntos de resultados limitados (top-40) com timeouts por query em plugins

`wilfred_bench` (`benches/bench_main.cpp`) mede os caminhos quentes em datasets sintéticos de 1K-100K nomes de arquivo (10 iterações cronometradas + 1 warmup, 5 para a inserção 100K) e relata Min / Median / Mean / P95 / Max milissegundos:

| Benchmark | O que mede |
|---|---|
| String pool: intern 100K | Throughput de interning (eficiência de memória do índice) |
| Tokenizer: tokenize 100K | Tokenização de nome de arquivo |
| Index insert: 1K / 10K / 100K | Bulk index build |
| Index query: 10K / 100K records | Posting-list lookup |
| Fuzzy search: 10K targets | Throughput de scoring Damerau-aware |
| Ranking: 10K scored hits | Weight scoring + top-40 selection |
| Filter chain: 10K records | Extension/kind filter evaluation |
| Math evaluation: 1K expressions | Calculator parse + eval |
| Snapshot save/load: 10K | Persistence round-trip |
| Memory reporting: 100K index | Index footprint accounting |

Execute no seu próprio hardware antes de citar números -- resultados dependem de CPU, disco e forma do dataset:

```bash
cmake --build build --config Release --target wilfred_bench
./build/wilfred_bench          # ou build/Release/wilfred_bench.exe
```

## Empacotamento

Push de tag `v*.*.*` roda `.github/workflows/release.yml`, que builda binários Release e anexa um arquivo por plataforma ao GitHub Release (veja `docs/building.md#packaging-what-ci-does`):

| Job | Artifacts |
|---|---|
| `windows-x64` | Portable `.zip`, signed `.msi` (WiX v3, per-user, no UAC), Chocolatey `.nupkg` (embedded portable, no download at install) |
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