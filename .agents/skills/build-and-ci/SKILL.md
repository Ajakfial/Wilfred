---
name: build-and-ci
description: Make small CMake or GitHub Actions tweaks for Wilfred. Use for warning flags, CI matrix updates, ctest timeouts, flaky-test retries, or similar build plumbing — not for new jobs, compiler upgrades, or release changes.
---

# Build and CI Tweaks

Build and CI changes are allowed only when they are small, local, and easy for
a human to validate. The release pipeline is off-limits.

## Allowed

- Adding a warning flag (`-W...`, `/W...`) to CMake
- Updating an existing GitHub Actions matrix entry (OS or compiler already in
  use)
- Adding a `ctest` timeout or retry for a flaky test
- Fixing a broken path or typo in a workflow
- Updating a build script flag that mirrors an existing CMake option

## Forbidden

- Adding a new CI job or stage
- Changing compiler versions or the minimum CMake version
- Modifying `.github/workflows/release.yml`
- Adding new dependencies, package managers, or caching layers
- Changing test selection, coverage thresholds, or signing steps
- Editing platform packaging scripts beyond a documented typo fix

## Before editing

1. Read the full workflow or CMake block, not just the changed line.
2. Check whether the same setting already exists elsewhere.
3. Confirm the change applies to the OS/compiler the human named.
4. Check the relevant docs (`docs/building.md`) for the documented behavior.

## Workflow

1. Make the smallest diff.
2. For CMake, reconfigure and build locally:

   ```bash
   cmake -S . -B build/local -DCMAKE_BUILD_TYPE=Debug -DWILFRED_BUILD_TESTS=ON
   cmake --build build/local --config Debug
   ctest --test-dir build/local --output-on-failure -C Debug
   ```

3. For workflows, re-read the YAML for indentation, `on:` triggers, and
   `needs:` references. Run `actionlint` if it is installed; otherwise ask the
   human to validate on a branch.
4. Report exactly which platform(s) you could not test.

## CI layout

| Workflow | Purpose |
|----------|---------|
| `.github/workflows/tests.yml` | Build + `ctest` on Windows, Linux, macOS; Android/iOS compile gates; overlay checks |
| `.github/workflows/lint.yml` | `clang-format` / `clang-tidy` on changed lines only |
| `.github/workflows/nightly.yml` | Nightly run |
| `.github/workflows/plugins.yml` | Plugin build |
| `.github/workflows/website.yml` | Website build |
| `.github/workflows/release.yml` | Release — **do not touch** |

## Guardrails

- Do not suppress warnings globally to make CI green.
- Do not add `continue-on-error` to hide a failure.
- Do not change a test's behavior in CI; fix or report the test.
- Do not touch secrets, signing, or permissions.
- If the fix needs a new job or a structural change, stop and hand it back.

## Done when

- The local configure/build/test path still works.
- The diff is limited to the requested plumbing change.
- A human has validated the CI behavior on a branch and owns the commit.
