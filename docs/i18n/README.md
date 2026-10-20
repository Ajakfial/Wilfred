# Translated READMEs

The English [README](../../README.md) is the source of truth. The translations
in this directory are AI-assisted and may lag behind it; when they disagree,
follow the English version.

| Language | Code | File |
|---|---|---|
| Deutsch | `de` | [README_de.md](README_de.md) |
| Español | `es` | [README_es.md](README_es.md) |
| Français | `fr` | [README_fr.md](README_fr.md) |
| Italiano | `it` | [README_it.md](README_it.md) |
| 日本語 | `ja` | [README_ja.md](README_ja.md) |
| 한국어 | `ko` | [README_ko.md](README_ko.md) |
| Nederlands | `nl` | [README_nl.md](README_nl.md) |
| Polski | `pl` | [README_pl.md](README_pl.md) |
| Português | `pt` | [README_pt.md](README_pt.md) |
| Русский | `ru` | [README_ru.md](README_ru.md) |
| 简体中文 | `zh` | [README_zh.md](README_zh.md) |

UI string translations are separate: they live in `lang/<code>.yml` and are
covered by [../localization.md](../localization.md).

## Updating a translation

```bash
cp README.md docs/i18n/README_<code>.md
```

Then translate the prose and leave code blocks, commands, paths, and links
unchanged. Keep the "made by Cursor AI" notice on line 1.
