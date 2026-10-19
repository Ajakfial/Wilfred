# SpokeApp -- MAUI front end + SpokeConsole -- cross-platform .NET host

Professional offline-transcriber UI for the spoke.cpp engine, plus a
terminal host for OSes MAUI doesn't reach (Linux).

## Structure

```
ui/
├── SpokeApp/                  .NET MAUI (Windows / MacCatalyst / iOS / Android)
│   ├── MauiProgram.cs          DI: SpokeNativeService → TranscriptionService → MainViewModel
│   ├── App.xaml(.cs)           dark-first design tokens
│   ├── AppShell.xaml(.cs)      single-page shell
│   ├── MainPage.xaml(.cs)      layout (bindings only) + pickers/clipboard/export glue
│   ├── Models/                 Segment / TranscriptionResult records
│   ├── Services/               ISpokeEngine, SpokeNativeService (P/Invoke + per-OS
│   │                           probing: spoke.dll / libspoke.so / libspoke.dylib),
│   │                           TranscriptionService
│   ├── ViewModels/             MainViewModel (INotifyPropertyChanged, zero MAUI refs)
│   ├── Resources/Icons/*.svg   hand-authored icon set + appicon + splash
│   └── Platforms/{Windows,Android,MacCatalyst}/  native-lib placement notes
└── SpokeConsole/              net8.0 console (Windows / Linux / macOS)
    ├── SpokeConsole.csproj     shares SpokeApp's Services/Models sources by link
    └── Program.cs              `info` + `transcribe [--json] [--threads N] [--beam N] [--lib DIR]`
```

## Build & run

```bash
# Console host (works on all three OSes; example: Linux)
dotnet run --project ui/SpokeConsole -- info --lib ./build/linux-ninja-release
dotnet run --project ui/SpokeConsole -- transcribe speech.wav --lib ./build/linux-ninja-release

# MAUI app: install workload (`dotnet workload install maui`), open
# SpokeApp.csproj in Visual Studio 2022+, copy the matching native lib next
# to the output (or set SpokeNativeService.LibrarySearchPath / SPOKE_LIBRARY_PATH).
```

No model downloads, no API keys, no network -- identical behaviour to the CLI
because all inference is the same C++ code.
