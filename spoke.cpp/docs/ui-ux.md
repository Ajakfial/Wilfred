# UI/UX — SpokeApp (MAUI)

## Principle

The MAUI layer does **zero signal processing**. It picks files, captures
microphone PCM, renders state beautifully, and P/Invokes `spoke.dll` for
everything else. A designer can restyle the whole app without touching
inference; an engine dev can swap the model without touching XAML.

## Screens (single-page professional transcriber)

- **Header**: Spoke logo (SVG) + model badge (`tiny.en · int8 · local`) + settings gear.
- **Drop zone**: file card with format hints (WAV/MP3 note), "Browse" + "Record" actions.
- **Live status**: waveform (SVG-driven bars), elapsed time, RTF chip after each run.
- **Results**: full transcript card (copy / export .txt / export .json) + segment
  list with `mm:ss` timestamps and confidence bars (green ≥ 0.8, amber ≥ 0.55,
  red below — matching the engine's 0.55 gate so the UI never contradicts it).
- **Settings sheet**: threads (auto/1–8), beam width (1–16), VAD sensitivity,
  all persisted to app preferences.

## Design system

- Dark-first theme: `#0E1116` surface, `#161B22` cards, `#58A6FF` accent,
  Inter-ish system fonts, 12 dp corner radii, 2 dp focus rings.
- All icons are **hand-authored SVG** in `Resources/Icons/`:
  `spoke-logo.svg`, `mic.svg`, `file-audio.svg`, `wave.svg`, `settings.svg`,
  `copy.svg`, `export.svg`, `check.svg`. No emoji, no raster assets.
- Accessibility: semantic headings, 4.5:1 text contrast, full keyboard path
  (Browse → Transcribe → Copy), screen-reader labels on every icon button.

## MVVM wiring

```
MainPage.xaml ─bindings─▶ MainViewModel (INotifyPropertyChanged, no MAUI refs in logic)
    │                            │
    │ commands                   │ uses
    ▼                            ▼
 code-behind (nav only)   TranscriptionService ─▶ SpokeNativeService (P/Invoke)
                                                        │  spoke_create/transcribe/free
                                                        ▼
                                                    spoke.dll (C++ engine)
```

`SpokeNativeService` is `IDisposable`, marshals UTF-8, parses the JSON
envelope into `TranscriptionResult` records, and surfaces engine errors as
typed `SpokeException`s. `TranscriptionService` validates extensions, guards
the 30 s chunk UX copy, and reports progress.

## Native library deployment

| Platform | Artifact | Placement |
|---|---|---|
| Windows | `spoke.dll` | next to app exe / `Platforms/Windows/` |
| Linux | `libspoke.so` (+ `libspoke.so.0`) | app dir, `~/.local/lib`, or system `ldconfig` path |
| macOS (MacCatalyst) | `libspoke.dylib` | app bundle `Frameworks/` (bundled resource) |
| Android | `libspoke.so` (arm64-v8a) | `Platforms/Android/jniLibs/` |
| iOS devices | static xcframework (roadmap) | `__Internal` — App Store forbids loose dylibs |

`SpokeNativeService` probes `--lib DIR`-equivalent `LibrarySearchPath` →
`SPOKE_LIBRARY_PATH` → app directory → OS loader, trying the right filename
per OS, so one C# codebase runs everywhere. Build each artifact from this
repo's CMake presets (`windows-vs2022`, `linux-ninja-release`,
`macos-ninja-release`) — no downloads, no keys, fully offline.

Linux has no MAUI target, so Linux .NET users run `ui/SpokeConsole`
(`net8.0`): `dotnet run --project ui/SpokeConsole -- transcribe a.wav
--lib ./build/linux-ninja-release`. Same engine, same service layer.
