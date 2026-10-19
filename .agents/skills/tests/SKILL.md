---
name: tests
description: Add or extend a Wilfred unit test under tests/. Use when covering existing behavior, adding an edge case, or writing a regression test for a bug that a human has already fixed or described.
---

# Adding Tests

Wilfred uses a tiny in-tree harness — not GoogleTest. Tests are plain
functions called from `main`, with `CHECK` macros from `tests/test.hpp`.

## Preconditions

- The human has stated the test name and the expected behavior.
- The behavior under test already exists, or the human has already made the
  production change. You do not write production logic.
- The test belongs to a module whose source is **not** forbidden. If the
  function under test lives in `src/search/`, `src/index/`, `src/query/`,
  `src/platform/`, or `src/ui/`, stop: AI-written tests for forbidden code
  still require the override process.

## Harness

`tests/test.hpp` provides:

| Macro | Use |
|-------|-----|
| `CHECK(expr)` | Boolean assertion |
| `CHECK_EQ(a, b)` | Equality |
| `CHECK_NEAR(a, b, eps)` | Floating-point closeness |

Results are counted in `wilfred::test::g_passes` / `g_fails`; the binary exits
non-zero on failure. There is no `--gtest_filter`; run the whole binary.

## Where a test goes

Existing test files are one per module, e.g. `tests/test_config.cpp`,
`tests/test_paths.cpp`, `tests/test_locale.cpp`, `tests/test_yaml.cpp`. Prefer
extending the matching file.

To add a **new** test function or file:

1. Create `tests/test_<module>.cpp` with `#include "test.hpp"` and the module
   header.
2. Declare `void test_<module>();` in `tests/test.hpp`.
3. Call `test_<module>();` from `tests/test_main.cpp` (before the summary
   print).
4. Add `tests/test_<module>.cpp` to `WILFRED_TEST_SOURCES` in
   `CMakeLists.txt` (around line 579).
5. On Windows, keep the build dir's config in mind (`-C Debug`).

## Shape of a test

```cpp
#include "test.hpp"

#include "wilfred/<module>/<header>.hpp"

void test_<module>() {
  using namespace wilfred;

  // Arrange — values chosen by the human
  Config cfg;

  // Act
  const bool ok = load_config_text(text, cfg, err);

  // Assert — expected behavior stated by the human
  CHECK(ok);
  CHECK_EQ(cfg.search.max_results, 12);
}
```

Follow the existing files: short functions, direct assertions, no custom test
framework, no sleeps unless the existing test already uses them.

## Rules

- Deterministic: no network, no real user config, no dependence on the current
  machine's index or locale.
- Use a temp directory and clean up if the test writes files.
- Test the behavior the human specified — do not invent assertions that could
  lock in unverified behavior.
- Do not change production code to make a test pass. Report the mismatch.
- Do not rename or delete existing tests.
- Keep the diff focused; do not reformat the file.

## Build and run

```bash
cmake --build build/local --target wilfred_tests --config Debug
ctest --test-dir build/local --output-on-failure -C Debug
```

Or use the platform script with `--tests` / `-Tests`.

## Done when

- The test compiles and passes.
- It fails if the behavior is broken (ask the human if unsure).
- A human has reviewed the assertions and owns the commit.
