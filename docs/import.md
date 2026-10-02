# Importing from other launchers

`wilfred import` copies settings from other launchers into `wilfred.yml`.
It covers all three desktop OSes: macOS (Alfred, Raycast), Windows
(PowerToys Run, Flow Launcher, Wox, Keypirinha, Listary) and Linux
(Ulauncher, Albert, KRunner/KDE, Rofi).

```
wilfred import --list
wilfred import --detect
wilfred import auto --dry-run
wilfred import alfred --from Alfred.alfredpreferences --dry-run
wilfred import flowlauncher --from Settings.json --overwrite
wilfred import --from ./shortcuts.json
```

## What gets imported

| Wilfred setting | Source examples |
|---|---|
| `macros:` + `quicklinks:` (custom web searches) | Alfred custom searches, Raycast quicklinks, Flow/Wox `SearchSources`, PowerToys URLs, Ulauncher shortcuts with `http` + `%s`, Albert engines, KDE `Query=` with `{@}`, Listary keywords |
| `snippets.items:` | Alfred snippets, Raycast snippet exports |
| `hotkey:` (global summon) | PowerToys `open_powerlauncher`, Flow `Hotkey`, Keypirinha `hotkey_run`, Listary launcher hotkey, Ulauncher `hotkey-show-app`, Albert `hotkey`, KDE `krunnerrc` |
| `browser.search_template:` | The launcher's default search (Flow `DefaultSearch`, Ulauncher `is_default_search`) |
| `ui.theme:` | Flow `Theme`, Ulauncher `theme-name`, Rofi `theme`, Raycast `theme` |
| `aliases:` / extra `quicklinks:` (commands) | Ulauncher non-URL shortcuts, Listary commands, Keypirinha aliases |

URL placeholders are normalized to Wilfred's `{query}` form: `%s`, `%q`,
`{q}`, `{searchTerms}`, KDE `\{@\}`/`{@}`, `$1` (single-arg) and plist
`&amp;` escapes. Keywords are lowercased and slugified (`GitHub Search` →
`githubsearch`) so `!kw` / `kw:` / `ql kw` all work: every imported web
search is added to **both** `macros:` and `quicklinks:` with the same
template.

## Merge vs overwrite

Default is merge: existing `wilfred.yml` values win, new keys are added,
identical values are skipped. Pass `--overwrite` to replace conflicting
templates. A safety backup is written to `wilfred.yml.pre-import.bak`
before every real (non-`--dry-run`) import.

Use `--dry-run` (`-n`) to preview counts without writing, and
`--no-hotkey` / `--no-searches` / `--no-snippets` / `--no-aliases` /
`--no-quicklinks` / `--no-theme` / `--no-browser` to skip categories.

## Per-launcher notes

- **Alfred**: point `--from` at the `Alfred.alfredpreferences` directory
  (every `*.plist` underneath is scanned) or at a single XML `prefs.plist` /
  workflow `info.plist`. Binary plists (`bplist`) are detected with a
  `plutil -convert xml1` hint. Script workflows without URLs are reported
  and skipped.
- **Raycast**: JSON quicklink exports (`[{name, link}]` or
  `{quicklinks: [...]}`). Snippet objects with `keyword` + `text`/`body`
  become Wilfred snippets.
- **PowerToys Run**: `settings.json` (`open_powerlauncher` hotkey object
  plus any search-like URLs).
- **Flow Launcher / Wox**: `Settings.json` (`Hotkey`, `SearchSources`
  with `Name`/`Url`/`ActionKeyword`, `DefaultSearch`, `Theme`).
- **Keypirinha**: `Keypirinha.ini` (`hotkey_run`, `[profile/*]` URLs) and
  sibling `WebSearch.ini` files.
- **Listary**: `Preferences.json` (`launcherHotkey`, `keywords[]`).
- **Ulauncher**: `shortcuts.json` (`[{name, keyword, cmd}]`, `%s` → `{query}`,
  `is_default_search` → default search) plus `settings.json`
  (`hotkey-show-app` like `<Primary>space`, `theme-name`).
- **Albert**: `albert.conf` (INI hotkey/theme) and `engines.json`
  (`[{name, trigger, url}]`, trailing spaces in triggers stripped).
- **KRunner**: `kuriikwsfilterrc` / `krunnerrc` (`Query=https://...\{@\}`).
- **Rofi**: `config.rasi` (`theme` → dark/light). The summon key usually
  lives in the WM config, so Wilfred keeps its own hotkey unless told
  otherwise.

`--from` also accepts any JSON/INI/plist file: the format is auto-detected
from the path and content, with a generic fallback that extracts hotkeys,
search URLs and shortcut arrays.

## Implementation

See `include/wilfred/import/import.hpp` (`supported_launchers()`,
`detect_launchers()`, `parse_text()`, `parse_directory()`,
`parse_file_auto()`, `apply_settings()`, `serialize_config()`) and
`Service::run_import*` in `include/wilfred/service/service.hpp`.
Tests live in `tests/test_import.cpp`.
