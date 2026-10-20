# (CETTE README A ÉTÉ CRÉÉE PAR CURSOR AI, TOUT LE RESTE, Y COMPRIS LE CODE, EST ÉCRIT DE MAIN D'HOMME.)

# Wilfred

**Site web:** https://ajakfial.github.io/Wilfred/

Wilfred est un moteur de recherche et lanceur d'applications de bureau rapide, léger et multiplateforme. Il est conçu comme une couche de recherche profonde du système : fichiers, dossiers, applications, intégration navigateur, calculatrice et requêtes filtrées depuis une seule barre de recherche invocable.

**RAPIDE. PROFOND. MODULAIRE. FAIBLE RESSOURCE. MULTIPLATEFORME. EXTENSIBLE.**

## Fonctionnalités

* Index incrémentiel, persistant des noms de fichiers/chemins/métadonnées avec journal d'écriture anticipée
* Observateurs de système de fichiers (ReadDirectoryChangesW, FSEvents, inotify) plus repli de réanalyse
* Correspondance floue, acronymes, classement par token/chemin, signaux de récence et fréquence
* Sécurité anti-fautes de frappe : Damerau (conscient des transpositions) *near-miss* sur noms, tokens et chemins, plus correction `Vouliez-vous dire` (commandes, historique, noms de fichiers)
* Autocomplétion en ligne : complétion fantôme (`→` pour accepter) + top-N candidats depuis l'historique, commandes et l'index
* Classement contextuel (dossiers récents, types de fichiers, heure de la journée, requêtes de session)
* Presse-papiers comme source secondaire (correspondance texte, chemins copiés, historique de clips)
* Gestionnaire de presse-papiers persistant (`clips [requête]`, épingler, `clips clear`, `clips url|email|path|code|ip`)
* Indexation de contenu pour code source, config et documents texte
* Extraction réelle de texte de documents (PDF, Office, RTF, HTML) dans l'index de contenu
* Recherche sémantique optionnelle : embeddings locaux (serveur/modèle llama.cpp ou hash intégré) + index vectoriel HNSW (`vectors.bin` à côté de WAL/snapshot) fusionné avec correspondance souple de trigrammes (`providers.semantic`, `embedding.*`)
* Assistant IA optionnel (`ai ...`/`ask ...`, `ai see ...` conscient de l'écran) avec vos propres clés : OpenAI, Anthropic, Gemini, Groq
* Snippets avec dossiers, placeholders `{date}`/`{clipboard}`/`{query}`, et abréviations globales optionnelles dans toute app
* Notes rapides + todos (`note ...`, `todo ...`, persistants dans le répertoire de données)
* Pomodoro / minuteurs compte à rebours + chronomètre (`timer`, `pomodoro`, `stopwatch`)
* Aperçus riches de fichiers (documents, CSV, images, dossiers) dans l'overlay (`F3`) plus `wilfred preview`
* Calendrier (`.ics`), contacts (`.vcf`) et notes (Markdown) sources + OCR optionnel via `tesseract` (`sources.*`)
* Overlay multiplateforme (Windows WebView2 topmost, macOS panneau flottant, Linux WebKitGTK avec fallback canvas X11 automatique, layer-shell ancré sur compositeurs Wayland, tous above/skip-taskbar) avec popover actions `Ctrl`/`⌘`+`K` et raccourcis correspondants dans les deux backends Linux
* Marque-pages, historique et onglets ouverts du navigateur (`bm`, piloté par provider)
* Énumération instantanée NTFS disque complet via journal USN sur Windows
* Fédération d'index officiel de l'OS (`os_search:`) : Windows Search, Spotlight, Tracker/Baloo, `locate` comblent les trous que l'index de Wilfred manque (`os <requête>` force, doublons supprimés)
* Volet d'aperçu dans l'overlay (`F3`) plus `wilfred preview`, et actions de résultats chaînées (`open+copy_path`) + workflows multi-étapes nommés (`workflows:`)
* Mini-résultats pour météo, heure, disque, RAM, CPU, processus, batterie, fenêtres, minuteurs, notes, médias, réseau, layouts, et plus
* Gestion fenêtres (`windows`, `minimize`/`maximize`/`close window`, snap) plus layouts fenêtre sauvegardés (`layout save`, `layout`)
* IA consciente de l'écran (`ai see ...` envoie capture comme entrée vision à votre fournisseur)
* Les notes rapides sont maintenant de vrais fichiers Markdown, recherchables comme cartes et via recherche notes
* Automatisation system-wide : hotkeys globaux supplémentaires (`hotkeys:`), déclencheurs CLI `wilfred workflow` / `wilfred exec`
* Macros de recherche (`!yt`, `gh`, `wiki`, plus modèles personnalisés dans config) + quicklinks paramétrés (`quicklinks:` avec `{1}`/`{*}`/`{query}`)
* Actions de contexte supplémentaires par app (`app_actions:`) au-dessus des actions fichier intégrées
* Recherche gros fichiers + candidats doublons depuis l'index (`large`, `dupes`)
* Conversion fichiers (`convert song.wav to mp3`, `convert photo.bmp to png` : natif WAV/RAW + BMP/PNG/PPM/TGA, ffmpeg pour mp3/ogg/flac/jpg/webp) + suppression arrière-plan (`bgremove photo.png` vers PNG transparent, pas de modèle nécessaire)
* Tueur de processus (`kill <pid|name>`), touches média natives (`media play/next/mute/vol` -- pas d'helpers à installer), outils réseau (`ping`, `dns`, `myip`), parole-vers-texte (`transcribe` via whisper CLI ou moteur spoke intégré)
* Filtres composables (`*.cpp in Projects`, `type:image`, `size:>10mb`, portées nommées)
* Découverte applications (Menu Démarrer / bundles `.app` / fichiers `.desktop`)
* Actions fichier riches (variantes copier chemin, hash SHA-256, zip, terminal/éditeur ici, nouveau fichier/dossier, Ouvrir avec)
* Détection navigateur par défaut, ouverture URL, et fallback recherche web
* Parseur d'expression sûr (arithmétique, fonctions, conversion unité/devise/couleur/date, uuid/base64/sha256/lorem/json, hex/dec/bin/oct, bits, regex/url/jwt)
* Plugins natifs et stdio, plus backends recherche supplémentaires via providers
* Calculatrice plus unités, devises, couleurs, date, et petits utilitaires dev
* Commandes système depuis l'overlay (`lock`, `sleep`, `shutdown`, `restart`, `logout`, vider corbeille)
* Bascules système (`wifi on/off`, `bluetooth toggle`, `volume 50/mute`, `brightness 70`) + deep-links réglages (`settings wifi/bluetooth/sound/display/...`) -- vrais backends OS sur Windows/macOS/Linux, erreurs honnêtes "non supporté" sur mobile/BSD
* Recherche gestionnaires paquets (`winget firefox`, `brew switch`, `apt vlc` -- Entrée installe, `Tab` copie commande; `packages` liste gestionnaires détectés)
* Réglages + setup (`settings config open/validate`, `settings edit <key> <value>`, `setup`, `wilfred setup --overwrite`, `wilfred config-get/set/reset`, `wilfred config-validate/open/path`) et backends recherche distants opt-in (`remotes:` -- vos endpoints seulement, off par défaut; par-source `headers:` + `max_results:`)
* Plugins avec confiance au premier usage (`plugins` cartes approbation, `wilfred plugin list/install/approve/revoke`, index `plugins.registry` optionnel, installations vérifiées sha256)
* Fenêtrage tuilé (`layout tile halves|thirds|grid|columns N|rows N|stack`, `wilfred tile …`) + auto-application changement moniteur (`layouts.auto_apply`, `auto_layout`, `monitor_layouts`)
* App Android (UI Kotlin + noyau C++ via NDK → `.apk`) : bouton W flottant, barre recherche, même index/recherche/classement -- voir `docs/android.md`
* App iOS (SwiftUI + même noyau C++ → `.app` simulateur) : même UI recherche qu'Android (entrée par icône app; iOS interdit boutons overlay), index sandbox uniquement -- voir `docs/ios.md`
* Sauvegarde/restauration locale et sync distant optionnel (`backup`, `restore`, `sync-push`, `sync-pull`)
* Import depuis autres lanceurs (`wilfred import --list/--detect/auto --dry-run/--overwrite`) : Alfred, Raycast, PowerToys Run, Flow Launcher/Wox, Keypirinha, Listary, Ulauncher, Albert, KRunner, Rofi -- voir `docs/import.md`
* Configuration YAML avec validation, indices `Vous vouliez dire` et erreurs lisibles
* Raccourci global : `Ctrl+Alt+W` (Windows/Linux), `⌘+Alt+W` (macOS), configurable
* Historique recherche local (optionnel; désactivable ou `wilfred history-clear`)
* UI overlay + CLI (`search`, `launch`, `index`, `status`, backup/sync, `import`)

## Compilation

Nécessite CMake 3.16+ et un compilateur C++20 (MSVC, Apple Clang, ou GCC).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Sous Linux, les en-têtes de développement X11 activent l'overlay (`libx11-dev`) ; sur sessions Wayland, l'overlay WebKit devient en plus une surface layer-shell native quand `libgtk-layer-shell-dev` est installé (Sway/wlroots, KDE Plasma -- GNOME n'a pas de protocole layer-shell, la fenêtre reste un toplevel simple).

## Exécution

```bash
# Démon en arrière-plan : overlay, indexeur, hotkey
wilfred

# Recherche unique contre l'index local
wilfred search firefox

# Recherche et ouvre le meilleur résultat
wilfred launch firefox

# Indexe les roots configurés maintenant
wilfred index

# Statistiques
wilfred status

# Importe paramètres d'un autre lanceur
wilfred import --list
wilfred import --detect
wilfred import auto --dry-run
wilfred import alfred --from Alfred.alfredpreferences --dry-run
```

La configuration utilisateur est créée au premier lancement :

| Plateforme | Config | Données index |
|---|---|---|
| Windows | `%APPDATA%\Wilfred\wilfred.yml` | `%LOCALAPPDATA%\Wilfred\` |
| macOS | `~/Library/Application Support/Wilfred/` | same |
| Linux | `~/.config/wilfred/wilfred.yml` | `~/.local/share/wilfred/` |
| BSD | `~/.config/wilfred/wilfred.yml` | `~/.local/share/wilfred/` |

Voir `config/wilfred.default.yml` pour le schéma complet (racines d'index, exclusions, répertoires système, poids de classement, alias, hotkey, historique, navigateur).

## Migration depuis un autre lanceur

`wilfred import` apporte raccourcis, recherches web personnalisées (en `macros:` + `quicklinks:`), snippets, alias, thème et le modèle de recherche par défaut dans `wilfred.yml`. La fusion est par défaut ; `--overwrite` remplace les conflits, `--dry-run` prévisualise, et une sauvegarde `.pre-import.bak` est écrite avant chaque vraie importation.

| OS | Sources supportées |
|---|---|
| macOS | Alfred (`Alfred.alfredpreferences` bundle), Raycast (quicklinks JSON) |
| Windows | PowerToys Run, Flow Launcher, Wox, Keypirinha, Listary |
| Linux | Ulauncher, Albert, KRunner / KDE, Rofi |

Voir `docs/import.md` (et `wilfred import --list`) pour le mapping par lanceur.

## Langage de requêtes (exemples)

| Requête | Signification |
|---|---|
| `firefox` | Recherche floue fichier/app |
| `25 * 42` | Calculatrice |
| `https://example.com` | Ouvrir URL |
| `? chats` / `g chats` | Recherche web dans navigateur par défaut |
| `*.cpp in Projects` | Extension + filtre répertoire |
| `type:image name:logo` | Type + nom fichier |
| `content:widget` / `intext foo` | Rechercher contenu indexé |
| `> notepad` | Style commande / lancement |
| `scope:home notes` | Groupe de répertoires nommé de la config |
| `meteo` / `meteo Paris` | Mini carte : local ou prévision ville |
| `speedtest` | Ping, download et upload en direct (`speedtest again` relance) |
| `heure` `disque` `disqueu` `ram` `cpu` | Horloge, disques, mémoire, processeur |
| `processus chrome` / `top` | Processus CPU, RAM, threads en direct (`kill <pid|name>` termine) |
| `capture` / `capture fenetre` / `capture region` | Capture plein écran, fenêtre ou région vers `Pictures/Wilfred` |
| `emoji sourire` / `symbole euro` | Sélecteur emoji/symboles (Entrée copie) |
| `100 usd en eur` / `fx 25 gbp jpy` | Conversion devise |
| `1 cs a g` / `1 tasse farine en g` | Volume↔masse cuisine (eau défaut, ingrédient optionnel) |
| `#ff5500` / `rgb(255, 85, 0)` / `couleur coral` | Conversion couleur (hex / rgb / hsl) |
| `15h est vers pst` / `now in tokyo` / `tz london` | Fuseaux horaires |
| `aujourd'hui + 7 jours` / `2024-01-31 + 1 mois` | Arithmétique dates |
| `uuid` / `base64 salut` / `sha256 abc` / `lorem 12` / `json {"a":1}` | Utilitaires dev (Entrée copie) |
| `hex 255` / `dec 0xff` / `0b1010` / `bit et 12 10` | Bases numériques + outils bit |
| `regex foo.* foobar` / `urlencode a b` / `jwt <token>` | Test regex, codec URL, décodage JWT |
| `timer 10m` / `pomodoro` / `chronometre start` | Compte à rebours, presets Pomodoro, chronomètre |
| `note acheter lait` / `notes` / `todo envoyer` / `todos` | Notes rapides + tâches (`todo done 1`) |
| `media play` / `media next` / `media mute` | Touches média natives + volume (Win media keys; mac HID + Music/Spotify/VLC; Linux MPRIS D-Bus natif + wpctl/pactl/amixer, playerctl optionnel) |
| `ping example.com` / `dns example.com` / `monip` | Ping, lookup DNS, IP publique |
| `large 10` / `dupes` | Plus gros fichiers + candidats doublons de l'index |
| `transcribe parle.mp3` / `dictate 10` | Voix-vers-texte pour audio mp3/mp4, dictée micro (`transcription.engine`: whisper CLI ou spoke intégré) |
| `convert song.wav to mp3` / `convert photo.bmp to png` | Conversion fichier (PCM natif + BMP/PNG/PPM/TGA, ffmpeg pour compressés) |
| `bgremove photo.png` / `bgremove photo.png 40 #ffffff` | Suppression fond vers PNG transparent (coins auto ou couleur explicite) |
| `fenetres chrome` / `minimiser spotify` | Lister, basculer, minimiser, maximiser, fermer, snap fenêtres |
| `layout save travail` / `layout travail` | Sauvegarder et restaurer layouts fenêtres |
| `define resilient` / `thesaurus happy` | Définition hors-ligne + synonymes (Entrée copie) |
| `pins` / `pin firefox` / `unpin firefox` | Favoris épinglés (boostés via `ranking.pinned`) |
| `event add Reunion equipe | demain 10am` | Créer événement calendrier (écrit `.ics`) |
| `contact add Jane Doe jane@x.com 555-0100` | Créer contact (écrit `.vcf`) |
| `rename ./photos photo-{n}.jpg` | Renommage en masse avec `{n}` `{name}` `{ext}` |
| `template python foo in ./dir` | Nouveau fichier depuis modèle (md/python/cpp/html/json/gitignore) |
| `clips image` / `clips path` | Historique presse-papiers filtré par type (text/url/email/path/code/ip/image) |
| `workflow revue` / `run revue` | Workflows multi-étapes nommés (`workflows` liste) |
| `ql docs bonjour` / `ticket:ABC-123` | Liens rapides paramétrés (`{1}`/`{*}`/`{query}`) |
| `lock` / `sleep` / `shutdown` / `restart` / `logout` / `empty trash` | Commandes système |
| `wifi` / `wifi off` / `bluetooth toggle` | Statut Wi-Fi + Bluetooth et on/off (Entrée applique) |
| `volume 50` / `volume mute` / `luminosite 70` | Volume niveau/mute + luminosité écran (Entrée applique) |
| `settings wifi` / `settings sound` / `settings` | Ouvrir pages réglages OS (wifi/network/bluetooth/sound/display/...) |
| `setup` / `config validate` / `config open` | État premier lancement + éditer/valider wilfred.yml (`wilfred setup`, `config-validate`) |
| `update` / `upgrade` | Statut nouvelle version du check démarrage (Entrée copie lien, `wilfred update` installe) |
| `settings edit search.max_results 40` / `settings get hotkey.key` | MAJ/lecture un réglage sur place, validé (`wilfred config-set/get`) |
| `plugins` / `plugins approve demo` | Liste plugins + approbations confiance (`wilfred plugin list/install/approve`) |
| `layout tile grid` / `tile halves` / `layout travail` | Tuile fenêtres ou applique layout sauvegardé (`wilfred tile`, `layouts.auto_apply` sur changement moniteur) |
| `winget firefox` / `brew switch` / `apt vlc` | Recherche paquets -- Entrée installe, `Tab` copie commande (`packages` liste gestionnaires) |
| `clip` / `clips` | Presse-papiers et clips récents |
| `!yt chats` / `gh wilfred` | Macros de recherche (`macros` liste) |

## Architecture

I/O plateforme derrière `wilfred/platform/native.hpp`. Modules noyau :

* `config` -- Parse YAML + validation schéma
* `index` -- Pool d'internement chaînes, records compacts, indexes inversés/trigrammes, WAL
* `fs` -- Walker, classification, volumes, watcher
* `search` -- Fuzzy, poids classement, filtres, cache requêtes
* `query` -- Classification + interpréteur
* `math` -- Calculatrice récursive-descente
* `apps` / `browser` / `history` / `hotkey` / `ipc` / `ui` / `service`
* `plugin` -- ABI natif `.dll`/`.so`/`.dylib` et plugins stdio, trust store, index registry
* `sync` -- Archives backup locales et push/pull distant optionnel
* `import` -- Importateur cross-OS paramètres lanceurs (hotkeys, recherches, snippets)
* `providers` -- Registry pour backends recherche supplémentaires (sémantique, bibliothèque navigateur, calendrier/contacts/notes, remotes, packages)

## Tests et Benchmarks

```bash
cmake --build build --config Release --target wilfred_tests wilfred_bench
./build/wilfred_tests          # ou build/Release/wilfred_tests.exe
./build/wilfred_bench
```

Tests couvrent CRUD index, persistance/récupération, fuzzy/classement/filtre, maths, classification requêtes, validation config, historique, chemins, IPC et découverte apps.

## Performance

Wilfred est un unique binaire C++20 sans bibliothèques tierces externes -- tout (JSON, YAML, wrapper mmap, CRC32) est implémenté dans l'arborescence, donc rien d'autre à installer (l'overlay Linux utilise les libs X11/WebKit système quand présentes, plus gtk-layer-shell pour ancrage Wayland natif, avec fallback canvas X11). La vitesse vient du design de l'index (voir `docs/architecture.md`) :

* Stockage compact de records memory-mapped avec pool d'internement chaînes (pas de chaînes chemin/nom dupliquées en RAM)
* Index trigramme + token pour que requêtes touchent listes de postings, pas scans complets
* Write-ahead log avec snapshots périodiques et récupération crash -- écritures sont des appends, lectures ne bloquent jamais sur persistance
* Mises à jour watchers FS incrémentielles et debounced au lieu de rescan
* Jeux de résultats bornés (top-40) avec timeouts par requête sur plugins

`wilfred_bench` (`benches/bench_main.cpp`) mesure les chemins chauds sur datasets synthétiques 1K-100K noms fichiers (10 itérations chronométrées + 1 warmup, 5 pour l'insertion 100K) et rapporte Min / Median / Mean / P95 / Max millisecondes:

| Benchmark | Ce qu'il mesure |
|---|---|
| String pool: intern 100K | Débit interning (efficacité mémoire index) |
| Tokenizer: tokenize 100K | Tokenisation noms fichiers |
| Index insert: 1K / 10K / 100K | Construction index en masse |
| Index query: 10K / 100K records | Lookup posting-lists |
| Fuzzy search: 10K targets | Débit scoring Damerau-conscient |
| Ranking: 10K scored hits | Weighted scoring + top-40 selection |
| Filter chain: 10K records | Évaluation filtre extension/type |
| Math eval: 1K expressions | Calculateur parse + eval |
| Snapshot save/load: 10K | Round-trip persistance |
| Memory reporting: 100K index | Comptabilité empreinte index |

Exécutez sur votre matériel avant de citer des chiffres -- résultats dépendent CPU, disque et forme dataset:

```bash
cmake --build build --config Release --target wilfred_bench
./build/wilfred_bench          # ou build/Release/wilfred_bench.exe
```

## Emballage

Un push de tag `v*.*.*` déclenche `.github/workflows/release.yml`, qui construit les binaires Release et attache une archive par plateforme au GitHub Release (voir `docs/building.md#packaging-what-ci-does`):

| Job | Artefacts |
|---|---|
| `windows-x64` | `.zip` portable, `.msi` signé (WiX v3, par-utilisateur, pas UAC), Chocolatey `.nupkg` (portable embarqué, pas de téléchargement à l'install) |
| `linux-x64` | `.tar.gz` portable |
| `freebsd-x64` | `.tar.gz` portable (FreeBSD release job; autres BSDs compilent depuis source -- voir `docs/bsd.md`) |
| `macos-arm64` | `.tar.gz` portable |
| `android` | `.apk` + `.aab` -- signés si secrets keystore existent, sinon `-unsigned` suffixé (voir `docs/android.md`) |
| `ios` | `.zip` simulateur non signé (voir `docs/ios.md`) |

Les archives desktop embarquent le binaire avec `ui/overlay/`, `README.md` et `config/wilfred.default.yml`. Les binaires Windows portent toujours le tampon éditeur `Wilfred Open Contributors` via VERSIONINFO ; la confiance Authenticode vient de PFX ou Azure Trusted Signing secrets quand configurés (voir `docs/signing.md`, `docs/installer.md`).

## Déploiement production

* **Exécutez le daemon, pas des one-shots.** `wilfred daemon` (le simple `wilfred`) est le processus longévif : indexeur + watcher + hotkey + overlay + serveur IPC. Lancez-le comme élément de connexion / entrée autostart (`wilfred autostart enable` enregistre ce binaire sur Windows/macOS/Linux/BSD) ; l'icône tray (Windows) l'arrête. Les sous-commandes CLI (`search`, `launch`, `status`, …) parlent au démon en cours via le socket IPC local quand disponible (voir `docs/cli.md`, `docs/ipc-and-api.md`).
* **La config est du code.** Premier lancement écrit un `wilfred.yml` validé (emplacements tableau ci-dessus) ; clés invalides échouent avec erreurs lisibles et indices `Vouliez-vous dire`, jamais defaults silencieux. Pour flottes, distribuez un `wilfred.yml` de base (roots, exclusions, poids, alias -- schéma complet dans `docs/configuration.md` et `config/wilfred.default.yml`) et laissez utilisateurs surcharger localement.
* **Données et DR.** Index, historique, clips, notes et logs vivent dans le répertoire de données par-OS (tableau ci-dessus). `backup` / `restore` déplacent l'état d'une machine ; `sync-push` / `sync-pull` le répliquent (voir `docs/sync-and-backup.md`). Testez les restaurations avant d'en avoir besoin.
* **Observez.** `wilfred status` rapporte santé index ; clés `logging.*` contrôlent verbosité/rétention logs ; l'API HTTP locale optionnelle (`api.*`, authentifiée) expose `/search`, `/status`, `/index`, `/launch` et endpoints backup/sync pour dashboards et automatisation.
* **Posture confidentialité.** Local-first : pas de compte, pas de télémétrie, pas d'appels réseau sauf si vous les activez (météo/IA/sync). Désactivez ou limitez historique via `history.*` où la politique l'exige ; fournisseurs IA ont besoin de vos propres clés API (`ai.*`, off par défaut).
* **Mobile est expérimental (depuis v23.0.5).** Les builds Android (`.apk`) et iOS (simulateur `.app`) installent et tournent mais ne sont pas encore prêts daily-driver en attendant plus de tests et développement -- déployez desktop d'abord (voir `docs/android.md`, `docs/ios.md`).