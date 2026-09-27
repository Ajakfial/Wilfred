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

## Near-term

* **Run tests in CI on every PR.** The only current workflow
  (`.github/workflows/release.yml`) builds release binaries on tagged
  pushes; it does not run `ctest` on pull requests yet (contributors run
  it locally per CONTRIBUTING.md). Adding a CI job for this is the
  highest-priority infra gap.
* **Enforce formatting/linting in CI.** `.clang-format` and `.clang-tidy`
  now exist; wire them into a CI check (format-diff on changed files
  rather than a full-tree reformat).
* **Packaged installers.** Releases currently ship as a zip/tar.gz per
  platform (see the release workflow). A Homebrew formula, a winget/
  Scoop manifest, and a `.deb`/AppImage would lower the install barrier.
* **Expand the mini and macro library.** The `RankPipeline`/mini
  extension points already support this without core changes — see
  [docs/query-language.md](docs/query-language.md) and
  `include/wilfred/search/minis.hpp`.

## Mid-term

* **Plugin discovery/registry.** Plugins ([docs/plugins.md](docs/plugins.md))
  are currently found only via local directories; a lightweight registry
  or index of known third-party plugins would make them easier to find
  and trust.
* **Config validation UX.** Friendlier diagnostics when `wilfred.yml`
  fails to parse or a key is misspelled, beyond the current
  human-readable YAML errors.
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
