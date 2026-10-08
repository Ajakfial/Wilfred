## What changed

<!-- Describe the change. Keep the PR focused on one logical change. -->

## Why

<!-- What problem does this solve? Link related issues, e.g. "Fixes #123". -->

## How I tested it

<!--
`ctest` should pass locally on at least one platform before requesting
review. Note which platform(s) you tested on below — reviewers may not be
able to test all three (Windows / macOS / Linux).
-->

- [ ] `ctest` passes locally
- Platform(s) tested: <!-- Windows / macOS / Linux -->

## Checklist

- [ ] This PR covers one logical change (see [CONTRIBUTING.md](../CONTRIBUTING.md))
- [ ] I added/updated tests under `tests/` for any behavioral change
- [ ] If this adds a config key, I updated `config.hpp`, `config.cpp`,
      `wilfred.default.yml`, and `docs/configuration.md`
- [ ] If this touches platform-specific code, it's guarded under
      `src/platform/` behind the existing `native.hpp` interface
- [ ] I did not introduce new third-party dependencies without prior discussion
- [ ] Docs (`README.md` / `docs/`) updated if user-facing behavior changed

## Plugin submission (only if this PR adds `plugins/<id>/`)

- [ ] `plugins/<id>/` holds `plugin.yml` (`id` = directory, `kind`, semver
      `version`, `description`, `permissions`), source, and `README.md`
      (see [plugins/README.md](../plugins/README.md))
- [ ] No binaries, zips, or `dist/` output committed (CI builds those)
- [ ] `python3 scripts/pack-plugins.py --validate-only` passes
- [ ] `plugins/registry.json` gains the entry (pack script or CI publish fills `artifacts`)

## Related issue

<!-- Fixes #... / Relates to #... -->
