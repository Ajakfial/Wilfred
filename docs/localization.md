# Localization

Wilfred's UI strings live in one place — `include/wilfred/locale/locale.hpp`
(keys) and `src/locale/locale.cpp` (compiled-in English fallback) — with
per-language overrides in `lang/<code>.yml`. No third-party i18n libraries;
the existing in-tree YAML parser reads the catalogs.

```yaml
ui:
  language: auto   # auto (OS locale), en, de, fr, es, ...
```

Shipped languages: English (`en`, always available), German (`de`), French
(`fr`), Spanish (`es`), Portuguese (`pt`), Italian (`it`), Dutch (`nl`),
Japanese (`ja`), Korean (`ko`), Chinese Simplified (`zh`), Russian (`ru`),
Polish (`pl`), Turkish (`tr`), Ukrainian (`uk`).
Unknown languages fall back to English per key, so a partial catalog stays
usable — untranslated strings simply remain English.

## How it works

1. At boot, `LocaleStore` resolves `ui.language` (`auto` sniffs the OS:
   `GetUserDefaultLocaleName` on Windows, the current `CFLocale` on macOS,
   `LANGUAGE`/`LC_ALL`/`LANG` on Linux/BSD, English on mobile).
2. It loads the first catalog found for the requested code, then the base
   language (`pt-BR` → `pt`): `<config dir>/lang/<code>.yml` (your
   overrides win), `lang/<code>.yml` next to the checkout/binary, then
   `share/wilfred/lang/<code>.yml` under the install prefix.
3. Code calls `tr("action.open")` (or `tr(key, {{"n", count}})`) and gets the
   catalog value, the compiled-in English, or the key itself — never empty.
4. Overlay chrome (search placeholder, empty states, action bar, Enter labels)
   travels in the `show` message's `strings` map (`overlay_show_json()`); the
   web UI falls back to English per key, so old hosts keep working.

Action IDs, payloads, query keywords (`define`, `pin`, `rename`, ...),
config keys, and file formats stay English — only human-readable labels are
translated.

## Adding a language

```bash
cp lang/en.yml lang/it.yml   # then translate the values
python3 scripts/check-lang.py
```

Rules (also in the header of `lang/en.yml`):

- Quote every value; never change the keys.
- Keep `{placeholders}` (`{n}`, `{q}`, `{syn}`, ...) and command syntax
  (`define <word>`, `timer 10m`, `{n} {name} {ext}`) byte-identical.
- `scripts/check-lang.py` verifies key parity across catalogs and against
  the compiled table; `test_locale` loads every shipped catalog with the
  real parser in CI.
- Regenerate the reference after changing English:
  `python3 scripts/generate-lang-en.py`.
