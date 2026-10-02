# Windows Publisher and Signing

Every Windows `wilfred.exe` — local or CI — carries the publisher name
**Wilfred Open Contributors** in its version resource. Authenticode trust
is a separate step with different guarantees.

## The three layers (do not confuse them)

1. **VERSIONINFO stamp (always on).** CMake configures
   `cmake/win_version_info.rc.in` into every Windows build, setting
   `CompanyName`, `LegalCopyright`, `ProductName`, and file version from
   `PROJECT_VERSION`. Check it with:

   ```powershell
   (Get-Item build\windows\Release\wilfred.exe).VersionInfo.CompanyName
   # Wilfred Open Contributors
   ```

   This is what Explorer shows under file Details. It does not stop
   SmartScreen.

2. **Authenticode signature (publisher proof).** `scripts/Sign-WindowsBinary.ps1`
   signs the binary so `Get-AuthenticodeSignature` reports a publisher
   instead of Unknown. Local dev certs use `CN=Wilfred Open Contributors`
   so the name matches CI.

3. **SmartScreen reputation (unknown-warning removal).** Only a
   Microsoft-trusted signature counts: Azure Trusted Signing or an OV/EV
   code-signing certificate from a public CA, **plus** built-up reputation
   for that publisher + file hash. A self-signed signature still shows the
   publisher name but still warns on machines that do not trust the cert.
   There is no code-only way around this; it is by design.

## Local builds

```powershell
# Build as usual; VERSIONINFO stamp is automatic.
./scripts/build-windows.ps1 -Config Release -Tests

# One-time: create a self-signed dev cert (publisher matches CI).
./scripts/New-DevCodeSignCert.ps1 -Install -ExportPath build\dev-signing.pfx

# Sign the build output.
./scripts/Sign-WindowsBinary.ps1 -ExePath build\windows\Release\wilfred.exe

# Or build and sign in one step (uses the installed dev cert).
./scripts/build-windows.ps1 -Config Release -Tests -Sign

# Verify.
./scripts/Sign-WindowsBinary.ps1 -ExePath build\windows\Release\wilfred.exe -VerifyOnly
```

`-Install` trusts the cert on your machine only (CurrentUser Root, no
elevation). Without it, the signature exists but verifies as Unknown on
your own box too. Never distribute the dev PFX or install it elsewhere;
generate a fresh one per machine.

## Release builds (CI)

`.github/workflows/release.yml` signs `wilfred.exe` after Build and before
Package, then verifies. Publisher comes from the signing identity:

- **Azure Trusted Signing (recommended).** Set repository secrets
  `AZURE_TENANT_ID`, `AZURE_CLIENT_ID`, `AZURE_CLIENT_SECRET`, plus either
  `AZURE_TRUSTED_SIGNING_ENDPOINT` or `AZURE_TRUSTED_SIGNING_ACCOUNT` with
  `AZURE_TRUSTED_SIGNING_PROFILE`. The workflow prefers this path when fully
  configured and installs `AzureSignTool` on demand.
- **PFX certificate.** Set `WILFRED_PFX_BASE64` (base64 of the .pfx) and
  `WILFRED_PFX_PASSWORD`. The workflow writes it to a temp file, signs with
  `signtool` + RFC3161 timestamp (`http://timestamp.digicert.com`), then
  deletes the temp file.
- **Neither present.** The job logs a warning and ships with the VERSIONINFO
  stamp only. Explorer shows the publisher, SmartScreen still warns. This
  keeps forks building without secrets.

The cert subject for releases must contain the publisher name
(`CN=Wilfred Open Contributors` or your validated org name) or the signed
binary will show a different publisher than local builds.

## What to expect where

| State | Explorer Details | `Get-AuthenticodeSignature` | SmartScreen on a stranger machine |
|---|---|---|---|
| Unsigned CI/local (VERSIONINFO only) | Wilfred Open Contributors | NotSigned | Warns (Unknown publisher) |
| Self-signed local | Wilfred Open Contributors | Valid locally once installed, Unknown elsewhere | Still warns elsewhere |
| Azure Trusted Signing / OV-EV | Wilfred Open Contributors (from cert) | Valid everywhere | Silent once reputation builds; may warn on first ever release |

## Troubleshooting

- `signtool.exe not found` — install Windows SDK or Visual Studio Build
  Tools; the script searches both kit paths plus PATH.
- `No signing identity found` — run `New-DevCodeSignCert.ps1` first, or pass
  `-Thumbprint` / `-PfxPath`.
- `UnknownError` after self-sign — expected until the cert is in Root on
  that machine; re-run the cert script with `-Install`.
- Timestamp failures in CI — transient network; the script uses
  `http://timestamp.digicert.com` for PFX mode and
  `http://timestamp.acs.microsoft.com` for Azure mode.
