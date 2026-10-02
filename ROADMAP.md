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

## Near-term

* **Run tests in CI on every PR.** The current workflows are
  `.github/workflows/lint.yml` (formatting/lint) and
  `.github/workflows/release.yml` (tagged release builds plus Windows
  sign/MSI/Choco steps); neither runs `ctest` on pull requests yet
  (contributors run it locally per CONTRIBUTING.md). Adding a CI job for
  this is the highest-priority infra gap.
* ~~**Enforce formatting/linting in CI.**~~ Done —
  `.github/workflows/lint.yml` runs `clang-format` and `clang-tidy` on
  changed C/C++ files for every PR (scoped to the diff, not a full-tree
  reformat, since most of the codebase predates these configs).
* **Packaged installers.** Windows now ships an `.msi` (per-user, WiX)
  and a Chocolatey package from the release workflow, alongside the
  zip/tar.gz per platform. Still open: a Homebrew formula, a
  winget/Scoop submission, and a `.deb`/AppImage.
* **Expand the mini and macro library.** Largely expanded (timers, notes,
  media, network, workflows, quicklinks — see
  [docs/query-language.md](docs/query-language.md) and
  `include/wilfred/search/minis.hpp`); the `RankPipeline`/mini extension
  points still accept more without core changes.

## Mid-term

* **Plugin discovery/registry.** Plugins ([docs/plugins.md](docs/plugins.md))
  are currently found only via local directories; a lightweight registry
  or index of known third-party plugins would make them easier to find
  and trust.
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
* **Remote/cloud index sources** as a `SearchProvider`
  ([docs/architecture.md](docs/architecture.md#extensibility-plugins-and-providers))
  for teams wanting to search shared drives or wikis.
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
