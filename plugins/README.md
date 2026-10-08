# Wilfred plugin gallery

This directory is the source of truth for the official plugin index
(`registry.json`, served at
`https://ajakfial.github.io/Wilfred/plugins/registry.json` and browsable on
the website's Plugins page).

- **Flagship plugins** live here as source (`dice`, `morse`, `password`,
  `cheatsheets`): single-file native plugins with zero dependencies.
- **Community plugins** are submitted the same way: a pull request that adds
  `plugins/<your-id>/` (see below). Merging to `main` triggers the
  `plugins.yml` workflow, which builds every plugin on Windows/macOS/Linux,
  smoke-tests the query protocol, and publishes the per-platform zips +
  updated `registry.json` automatically. You only submit **source**.

## Submitting a plugin (PR format)

A plugin PR must contain exactly one new directory, `plugins/<id>/`, with:

| File         | Required | Contents                                                        |
| ------------ | -------- | --------------------------------------------------------------- |
| `plugin.yml` | yes      | `id` (= directory name), `kind` (`native`/`stdio`), `version` (semver), `description`, `permissions` (list, may be empty) |
| source       | yes      | Native: one or more `.c`/`.cpp` files, C ABI v1, no third-party deps. stdio: an executable script + `command` in the manifest |
| `README.md`  | yes      | What it does, example queries, build/run instructions            |

Rules:

- `id` must match `[a-z0-9][a-z0-9-]*` and equal the directory name.
- Native code must be warning-clean under `-Wall -Wextra` (GCC/Clang) and
  `/W4` (MSVC), portable C99/C++17, no network/file writes outside the query
  protocol. Declare every permission the plugin needs in `permissions:`
  (`network`, `fs-read`, ...); undeclared behavior fails review.
- `version` starts at `1.0.0` and bumps on every behavior change (trust
  re-approval keys off version+sha256+permissions).
- stdio scripts must answer within `timeout_ms` (default 400ms) — keep
  startup fast (no heavy interpreters on the hot path if avoidable).
- Never commit binaries, zips, or `dist/` output — CI builds and publishes
  those. Run `python3 scripts/pack-plugins.py --validate-only` before
  pushing.

Review follows [CODEOWNERS](../.github/CODEOWNERS) (platform/tooling →
@SGizek). See [docs/plugins.md](../docs/plugins.md) for the ABI, the
stdio protocol, and the trust model.
