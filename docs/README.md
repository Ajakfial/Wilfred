# Wilfred Documentation

This directory contains in-depth documentation for Wilfred, a fast,
lightweight, cross-platform desktop search engine and application launcher.
The top-level [README.md](../README.md) is a quick-start; the documents here
go deeper on how each subsystem works, for both users configuring Wilfred and
developers extending it.

| Document | Covers |
|---|---|
| [android.md](android.md) | Android app (Kotlin UI + C++ core via NDK) build and packaging |
| [architecture.md](architecture.md) | How the modules fit together, from hotkey to overlay |
| [automation.md](automation.md) | System-wide hotkeys, CLI triggers, and window/layout workflow steps |
| [benchmarks.md](benchmarks.md) | Microbenchmark suite and how to run it |
| [building.md](building.md) | Full build instructions, options, and CMake presets for all platforms |
| [bsd.md](bsd.md) | FreeBSD / OpenBSD / NetBSD notes |
| [cli.md](cli.md) | Every `wilfred` subcommand |
| [configuration.md](configuration.md) | Full reference for `wilfred.yml`, every key and default |
| [cross-platform-verification.md](cross-platform-verification.md) | Cross-platform build and behavior verification matrix |
| [import.md](import.md) | Importing settings from other launchers |
| [indexing.md](indexing.md) | How the index is built, stored, watched, and recovered |
| [installer.md](installer.md) | Windows MSI, Setup.exe wizard, and Chocolatey packaging |
| [ios.md](ios.md) | iOS app (SwiftUI + C++ core) build and packaging |
| [ipc-and-api.md](ipc-and-api.md) | The local IPC protocol and the optional local HTTP API |
| [localization.md](localization.md) | UI languages, catalog files, and adding a translation |
| [plugins.md](plugins.md) | Writing native and stdio plugins, the manifest format |
| [query-language.md](query-language.md) | Search syntax: fuzzy matching, filters, macros, minis, scopes |
| [ranking.md](ranking.md) | How search results are scored and ordered |
| [repo-layout.md](repo-layout.md) | Where top-level files live and why |
| [signing.md](signing.md) | Publisher identity and Authenticode trust |
| [sync-and-backup.md](sync-and-backup.md) | Backup archive format and remote sync |
| [i18n/](i18n/README.md) | Translated READMEs and the language index |

If something you need isn't covered, the header files under
`include/wilfred/<module>/` are the source of truth -- each doc below points
to the specific headers it summarizes.
