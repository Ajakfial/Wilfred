# Wilfred Setup host

Win32 + WebView2 host for the TypeScript setup wizard. It embeds the built
wizard HTML and the MSI as resources, shows the wizard, and installs the MSI
with `MsiInstallProductW` while forwarding progress to the page.

The user-facing behavior (flow, fallbacks, signing) is documented in
[`docs/installer.md`](../../docs/installer.md). This file covers the code.

## Layout

| File | Purpose |
| --- | --- |
| `src/main.cpp` | Window, WebView2 setup, resource loading, bridge, CLI modes |
| `src/msi_runner.cpp/.h` | `MsiInstallProductW` + external UI handler (progress, errors) |
| `src/json.cpp/.h` | Minimal JSON string escaping/extraction for the bridge |
| `src/app.rc.in` | Resource template: HTML, MSI, icon, VERSIONINFO |
| `src/app.manifest` | Per-monitor DPI awareness and asInvoker execution level |
| `src/resource.h` | Resource IDs shared by RC and C++ |
| `src/version.h.in` | Generated version constants |
| `CMakeLists.txt` | Standalone build (does not build the main app) |

Paths and version are injected by `scripts/Build-Setup.ps1` with
`-DSETUP_MSI`, `-DSETUP_HTML`, `-DSETUP_ICON`, `-DSETUP_VERSION`, and
`-DWEBVIEW2_ROOT`. The wizard must be built first (`installer/ui` →
`dist/index.html`); WebView2 caps `NavigateToString` at 2 MB.

## Modes

```text
wilfred-setup.exe              wizard
wilfred-setup.exe /S           silent install, returns the MSI result code
wilfred-setup.exe --check      verify embedded MSI + HTML, then exit
wilfred-setup.exe --ui-check   load the wizard in WebView2, then exit
wilfred-setup.exe --version    print the setup version
wilfred-setup.exe --help
wilfred-setup.exe --msi PATH   install a specific MSI instead of the embedded one
```

`--check` is safe on machines without WebView2 and is what CI runs as a hard
gate. `--ui-check` needs the WebView2 Runtime and is a soft check in CI.

## Bridge

The page sends JSON via `window.chrome.webview.postMessage(...)`. Handled
messages:

| `type` | Fields | Effect |
| --- | --- | --- |
| `ready` | — | host confirms the UI loaded; answers `init` |
| `install` | `launch?` | starts the MSI install |
| `close` | — | closes the window with the current exit code |
| `openLog` | — | opens `%TEMP%\wilfred-setup.log` |

The host pushes `init` (version, install dir), `status`, `progress`
(`percent`, `action`), `completed` (`code`, `launch`), and `failed`
(`message`, `code`).

## Notes

- The MSI is extracted to `%TEMP%\wilfred-setup-<pid>.msi` and deleted on
  exit unless the WebView2 fallback had to hand it to `msiexec`.
- If extraction fails, the host looks for `wilfred*.msi` next to the exe.
- `INSTALLUILEVEL_PROGRESSONLY` keeps the MSI from opening its own windows;
  the external UI handler parses `INSTALLMESSAGE_PROGRESS` and action data.
- An external UI handler must take `LPCWSTR`, matching `INSTALLUI_HANDLERW`.
- The manifest is merged with `/MANIFESTINPUT`, not embedded as a second
  `RT_MANIFEST` resource (the linker already emits one; a duplicate fails
  with CVT1100).
- Paths written into the generated `.rc` need doubled backslashes because
  `rc.exe` processes C-style escapes inside quoted file names.
