# Roadmap

This is a living snapshot of where Wilfred is headed, not a fixed
commitment or a schedule. Priorities shift based on contributor
availability and user feedback — if something below matters to you,
open an issue (see [CONTRIBUTING.md](CONTRIBUTING.md)) so it can be
prioritized and discussed before someone starts building it.

Review of roadmap-affecting PRs follows [CODEOWNERS](.github/CODEOWNERS):
engine/index/search/query/math changes go through @Ajakfial, platform/UI/
service/tooling changes go through @SGizek.

## Shipped

The feature set described in [README.md](README.md) — incremental
indexing with a write-ahead log, filesystem watchers on all three
platforms, fuzzy/token/path ranking with context-aware signals, minis,
macros, filters, plugin support (native + stdio), local backup/sync, the
overlay UI, and the CLI — is implemented today. See
[docs/](docs/README.md) for how each subsystem works.

Also shipped since: productivity minis (countdown timers, Pomodoro
presets, stopwatch, quick notes, todos), process listing plus `kill`,
number-base and bit tools, regex tester, URL codec and JWT decode next to
the existing uuid/base64/sha256/lorem/json utilities, native media
controls (Windows media keys, macOS HID keys plus Music/Spotify/VLC
fallback, Linux native MPRIS D-Bus with optional `playerctl` and a
`wpctl` → `pactl` → `amixer` volume chain), network tools (`ping`,
`dns`, `myip`), clipboard history filtered by type (`clips url|email|
path|code|ip`), largest-file and duplicate-candidate finders over the
index, named multi-step workflows, parameterized quicklinks, per-app
context actions, and friendlier config diagnostics with `Did you mean`
hints. Windows releases carry the `Wilfred Open Contributors` publisher
stamp (VERSIONINFO), Authenticode signing when secrets are configured
(see [docs/signing.md](docs/signing.md)), a per-user `.msi` installer,
and a Chocolatey package (see [docs/installer.md](docs/installer.md)).

Also shipped since: system toggles (`wifi`, `bluetooth`, `volume`,
`brightness` with real per-OS backends) plus settings deep-links
(`settings wifi/bluetooth/sound/...`), a `settings` config editor
(`settings edit <key> <value>`, `wilfred config-set/get`), the `setup`
first-run wizard (`wilfred setup`), opt-in remote search backends
(`remotes:` with per-source headers and caps), window tiling presets
(`layout tile …`, `wilfred tile`) with monitor-change auto-apply
(`layouts.auto_apply`), plugin trust-on-first-use approvals plus an
optional registry index (`wilfred plugin list/install/approve`), and
native Android/iOS handling for the toggle/settings cards (system
settings intents, `AudioManager` volume). Package-manager search
(`winget`/`brew`/`apt`/`choco`/`flatpak`/`pacman` with install on Enter)
and PR-gated tests (`.github/workflows/tests.yml`, desktop + overlay
bundle check) also shipped. See
[docs/query-language.md](docs/query-language.md),
[docs/configuration.md](docs/configuration.md), and
[docs/plugins.md](docs/plugins.md).

Also shipped since: encrypted sync (`sync.encrypt` with `password` /
`key_file`, dependency-free SHA-256 KDF + keystream), custom overlay themes
(`ui.accent`, `ui.font_size`), pinned favorites (`pin`/`pins` plus
`ranking.pinned`), an offline dictionary/thesaurus (`define`), clipboard
image and path clips (`clips image`, `clips path`), calendar/contact
creation (`event add`, `contact add`), global snippet expansion on macOS
(CGEventTap) and X11 Linux/BSD, bulk file operations (`rename`, `move_to`,
`new_from_template`), UI localization (`ui.language` with `lang/*.yml`
catalogs: en, de, fr, es, pt, it, nl), snapshot format v3 with persisted
postings (`index.format`), hybrid semantic fusion weights, and the official
plugin gallery (`plugins/` + per-OS registry artifacts + flagship plugins
`dice`, `morse`, `password`, `cheatsheets`).

## Near-term

* ~~**Run tests in CI on every PR.**~~ Done —
  `.github/workflows/tests.yml` builds + runs `ctest` on Windows, Linux
  and macOS for every PR and `main` push (BSD stays on nightly — VM
  runners are slow), plus an overlay job (`typecheck`, bundle build, and
  a stale-`dist/` check so TS changes always ship their bundle).
* ~~**Enforce formatting/linting in CI.**~~ Done —
  `.github/workflows/lint.yml` runs `clang-format` and `clang-tidy` on
  changed C/C++ files for every PR (scoped to the diff, not a full-tree
  reformat, since most of the codebase predates these configs).
* **Packaged installers.** Windows now ships an `.msi` (per-user, WiX)
  and a Chocolatey package from the release workflow, Linux a `.deb` and
  an AppImage, alongside the zip/tar.gz per platform. Still open: a
  Homebrew formula and a winget/Scoop submission.
* **Expand the mini and macro library.** Largely expanded (timers, notes,
  media, network, workflows, quicklinks — see
  [docs/query-language.md](docs/query-language.md) and
  `include/wilfred/search/minis.hpp`); the `RankPipeline`/mini extension
  points still accept more without core changes.

## Mid-term

* ~~**Plugin discovery/registry.**~~ Done — `plugins.registry` points at a
  JSON index for `wilfred plugin list/install` (sha256-verified), and
  trust-on-first-use approvals (`wilfred plugin approve`, `plugins`
  overlay cards) gate new/changed plugins (see
  [docs/plugins.md](docs/plugins.md)).
* ~~**Config validation UX.**~~ Done — unknown keys suggest the closest
  valid key, type errors show what was got versus expected with an
  example, and `workflows:`/`quicklinks:`/`app_actions:` validate action
  ids and placeholders instead of failing silently.
* **Optional encrypted sync.** The backup/sync format
  ([docs/sync-and-backup.md](docs/sync-and-backup.md)) is a plain
  dependency-free binary container; encryption-at-rest for the archive
  would need discussion first, per the project's no-new-dependencies
  policy in CONTRIBUTING.md.
* **`.clang-format` rollout to existing files.** Land in small,
  reviewable chunks per module rather than one large reformat, to keep
  diffs reviewable (see CONTRIBUTING.md's guidance on focused PRs).

## Long-term / exploratory

* **Additional platform backends** beyond Windows/macOS/Linux (e.g. BSD),
  behind the existing `platform/native.hpp` abstraction.
* ~~**Remote/cloud index sources** as a `SearchProvider`
  ([docs/architecture.md](docs/architecture.md#extensibility-plugins-and-providers))
  for teams wanting to search shared drives or wikis.~~ Done — the opt-in
  `remotes:` backends (`include/wilfred/search/remote.hpp`) query
  team-owned HTTP endpoints with per-source headers and caps (see
  [docs/configuration.md](docs/configuration.md)).
* **Alternative on-disk index formats** for very large indexes (millions
  of files) — this changes the WAL/snapshot format described in
  [docs/indexing.md](docs/indexing.md) and needs a design issue first,
  per CONTRIBUTING.md.

## Explicitly not planned

* Adding third-party runtime dependencies to the core (`src/core`,
  `src/config`) without a design discussion — the dependency-free,
  low-resource footprint is a stated project goal.
* A hosted/cloud-only mode that requires an account — Wilfred is a local
  search tool first; sync is opt-in and points at an endpoint you control.
