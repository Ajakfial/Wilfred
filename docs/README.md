# Wilfred Documentation

This directory contains in-depth documentation for Wilfred, a fast,
lightweight, cross-platform desktop search engine and application launcher.
The top-level [README.md](../README.md) is a quick-start; the documents here
go deeper on how each subsystem works, for both users configuring Wilfred and
developers extending it.

| Document | Covers |
|---|---|
| [architecture.md](architecture.md) | How the modules fit together, from hotkey to overlay |
| [building.md](building.md) | Full build instructions, options, and CMake presets for all platforms |
| [cli.md](cli.md) | Every `wilfred` subcommand |
| [configuration.md](configuration.md) | Full reference for `wilfred.yml`, every key and default |
| [query-language.md](query-language.md) | Search syntax: fuzzy matching, filters, macros, minis, scopes |
| [indexing.md](indexing.md) | How the index is built, stored, watched, and recovered |
| [ranking.md](ranking.md) | How search results are scored and ordered |
| [plugins.md](plugins.md) | Writing native and stdio plugins, the manifest format |
| [ipc-and-api.md](ipc-and-api.md) | The local IPC protocol and the optional local HTTP API |
| [sync-and-backup.md](sync-and-backup.md) | Backup archive format and remote sync |

If something you need isn't covered, the header files under
`include/wilfred/<module>/` are the source of truth — each doc below points
to the specific headers it summarizes.
