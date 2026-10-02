# Windows Installer (.msi)

Releases ship `wilfred-<tag>-windows-x64.msi` alongside the portable zip.
The installer is built with WiX Toolset v3 from
`packaging/wix/Wilfred.wxs` and carries the same publisher identity as
the binary (see [signing.md](signing.md)).

## What installing does

- Installs per-user to `%LocalAppData%\Wilfred` — no UAC elevation.
- Adds Start Menu shortcuts (Wilfred + Uninstall).
- Appends the install dir to the **user** `PATH` (removed on uninstall),
  so `wilfred search ...` works from any terminal.
- Registers Publisher **Wilfred Open Contributors** in Add/Remove Programs.
- Major upgrades in place: installing a newer `.msi` replaces the old one.
  The `UpgradeCode` in `Wilfred.wxs` is stable forever — never change it,
  or upgrades become side-by-side installs.
- Optional checked-by-default **Launch Wilfred** on the final wizard page.

## Build it

```powershell
# Release-style (tag version comes from -MsiVersion, defaults to CMake version)
./scripts/build-windows.ps1 -Config Release -Msi -MsiVersion v1.2.3

# Plus Authenticode-sign the .msi with your dev cert
./scripts/build-windows.ps1 -Config Release -Msi -Sign
```

Direct form (e.g. in CI after the Package step staged the files):

```powershell
./scripts/Build-Msi.ps1 -Staging dist\wilfred-v1.2.3-windows-x64 `
    -Out dist\wilfred-v1.2.3-windows-x64.msi -Version v1.2.3
./scripts/Build-Msi.ps1 -ValidateOnly   # no WiX needed: template self-checks
```

Requires WiX Toolset v3 (`candle`/`light`/`heat`): preinstalled on GitHub
`windows` runners; local fallback tries `choco install wixtoolset`, else
install it manually. Product version is sanitized from the tag to numeric
`X.Y.Z` (major/minor 0–255, build capped at 65535).

The release workflow signs the `.msi` with the same identity as the `.exe`
(PFX secrets first, Azure Trusted Signing second) and attaches it to the
GitHub Release. Unsigned MSIs install fine; SmartScreen still warns until
a Microsoft-trusted signature plus reputation (see [signing.md](signing.md)).

## Customization points

- App icon: drop an `icon.ico` at `packaging/wix/icon.ico` — the build
  picks it up automatically for shortcuts and Add/Remove Programs.
  Without it the installer builds icon-less (valid, just generic).
- Scope is intentionally per-user. Switching to per-machine would require
  `InstallScope="perMachine"`, elevation, and HKLM/Program Files moves —
  a deliberate change, not a flag.

## Chocolatey package (.nupkg)

Releases also ship an embedded portable Chocolatey package built from the
same staging dir — no downloads at install time, works offline.

- Manifest: `packaging/choco/wilfred.nuspec.template` (id `wilfred`,
  authors/owners `Wilfred Open Contributors`, MIT). Version is normalized
  from the tag (`v1.2.3` → `1.2.3`).
- Hooks: `packaging/choco/tools/chocolateyinstall.ps1` creates the Start
  Menu shortcut (the `wilfred.exe` shim is automatic);
  `chocolateyuninstall.ps1` removes the shortcut.
- Build: `./scripts/Build-Choco.ps1 -Staging <dir> -OutDir dist -Version v1.2.3`
  (`-ValidateOnly` checks the template and hooks without `choco.exe`), or
  `./scripts/build-windows.ps1 -Choco` for a local one-command build.
- The release workflow attaches the `.nupkg` to the GitHub Release and
  pushes it to the community feed only when the `CHOCO_API_KEY` secret is
  set — otherwise it logs a warning and skips the push.
- If the `wilfred` id is taken on the community feed (first push answers
  409), rename the id in the template and here, e.g. `wilfred-launcher`.
