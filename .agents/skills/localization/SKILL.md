---
name: localization
description: Translate or update Wilfred's lang/*.yml catalogs. Use when the human has added or changed an English string and wants translations, or when a catalog needs parity fixes.
---

# Localization

Wilfred ships 14 language catalogs. English is the compiled-in fallback; every
other catalog overrides it per key. The human approves keys and reviews
translations.

## Preconditions

- The English key already exists in `src/locale/locale.cpp` (`kEnglish`).
- The human has approved adding or changing the key.
- `lang/en.yml` has been regenerated after the English change:

  ```bash
  python3 scripts/generate-lang-en.py
  ```

## Workflow

1. Read `lang/en.yml` and the target `lang/<code>.yml`.
2. Translate **values only**. Never change keys, order, or the file's header
   comments.
3. Keep the catalog's key set and order identical to `en.yml`.
4. Run the checker:

   ```bash
   python3 scripts/check-lang.py
   ```

5. Run the locale test:

   ```bash
   ctest --test-dir build/local --output-on-failure -C Debug
   ```

   (`test_locale` loads every shipped catalog with the real parser.)
6. Hand the diff to the human for spot-checking.

## Hard rules

- Quote every value with double quotes.
- Keep placeholders byte-identical: `{n}`, `{q}`, `{syn}`, `{name}`, `{ext}`,
  etc. Never translate or reorder them.
- Keep command syntax unchanged: `define <word>`, `timer 10m`,
  `{n} {name} {ext}`, flags, action IDs, payloads, config keys, query
  keywords, and file formats stay English.
- Preserve trailing punctuation and spacing conventions of the target
  language, but do not add markup.
- Do not add or remove keys to "fix" a missing translation; untranslated keys
  fall back to English.
- Do not machine-translate without human review.
- Do not translate `lang/en.yml` itself into another language, and do not
  edit the compiled table unless the human asks.

## Supported catalogs

`de`, `en`, `es`, `fr`, `it`, `ja`, `ko`, `nl`, `pl`, `pt`, `ru`, `tr`, `uk`,
`zh`.

## Guardrails

- If `check-lang.py` reports a parity or ordering error, stop and report it;
  do not reorder unrelated keys to hide the failure.
- If a string contains code, a path, or a command example, ask whether it
  should remain untranslated.
- Do not touch locale code (`src/locale/locale.cpp`) beyond the English table
  the human already changed.
- Do not generate commit messages or PR descriptions.

## Done when

- `check-lang.py` reports `OK`.
- `test_locale` passes.
- A human has spot-checked the translations and owns the commit.
