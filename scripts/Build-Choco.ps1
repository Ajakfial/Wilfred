# Builds the Wilfred Chocolatey package (.nupkg), embedded portable style.
#
# Inputs are the same staging layout the release workflow zips:
#   <Staging>\wilfred.exe
#   <Staging>\ui\overlay\...
#   <Staging>\README.md
#   <Staging>\wilfred.default.yml
#
# The nupkg embeds those files under tools/ (no downloads at install time,
# works offline). Publisher is fixed to Wilfred Open Contributors in the
# nuspec template. Version is normalized from the release tag (leading v
# stripped, e.g. v1.2.3 -> 1.2.3).
#
# Requires choco.exe (preinstalled on GitHub windows runners).
#
# Usage:
#   ./scripts/Build-Choco.ps1 -Staging dist\wilfred-v1.2.3-windows-x64 `
#       -OutDir dist -Version v1.2.3
#   ./scripts/Build-Choco.ps1 -ValidateOnly   # no choco needed: template + hooks self-check
[CmdletBinding()]
param(
    [string]$Staging = '',
    [string]$OutDir = 'dist',
    [string]$Version = '',
    [switch]$ValidateOnly
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Template = Join-Path $Root 'packaging\choco\wilfred.nuspec.template'
$HooksDir = Join-Path $Root 'packaging\choco\tools'

function Get-ChocoVersion([string]$Tag) {
    $t = $Tag.Trim().TrimStart('v', 'V')
    if ($t -notmatch '^\d+(\.\d+){0,3}(-[0-9A-Za-z.\-]+)?$') {
        throw "Version $Tag is not a valid Chocolatey/NuGet version (expected like v1.2.3)."
    }
    return $t
}

# --- Template + hooks self-checks (run even in ValidateOnly) ---
if (-not (Test-Path $Template)) { throw "Nuspec template not found: $Template" }
$probe = (Get-Content $Template -Raw) -replace '%%VERSION%%', '1.0.0'
try {
    [xml]$nuspec = $probe
} catch {
    throw "Nuspec template is not well-formed XML: $($_.Exception.Message)"
}
$meta = $nuspec.package.metadata
if (-not $meta) { throw 'Nuspec template has no package/metadata element.' }
if ($meta.id -ne 'wilfred') { throw "Nuspec id is $($meta.id), expected wilfred." }
foreach ($field in @('authors', 'owners')) {
    if ($meta.$field -ne 'Wilfred Open Contributors') {
        throw "Nuspec $field is $($meta.$field), expected Wilfred Open Contributors."
    }
}
$files = $nuspec.package.files.file
if (-not $files) { throw 'Nuspec template embeds no files (expected tools\**).' }
foreach ($hook in @('chocolateyinstall.ps1', 'chocolateyuninstall.ps1')) {
    $p = Join-Path $HooksDir $hook
    if (-not (Test-Path $p)) { throw "Missing choco hook: $p" }
    $errs = $null; $toks = $null
    [void][System.Management.Automation.Language.Parser]::ParseFile($p, [ref]$toks, [ref]$errs)
    if ($errs.Count -gt 0) { throw "Syntax error in ${hook}: $($errs[0].Message)" }
}
$installHook = Get-Content (Join-Path $HooksDir 'chocolateyinstall.ps1') -Raw
if ($installHook -notmatch 'Install-ChocolateyShortcut') {
    throw 'chocolateyinstall.ps1 must create the Start Menu shortcut via Install-ChocolateyShortcut.'
}
Write-Host 'Choco template checks passed (publisher, embedded files, hook syntax).'

if ($ValidateOnly) { exit 0 }

if (-not $Staging) { throw '-Staging is required (directory with wilfred.exe + ui/overlay + docs).' }
if (-not $Version) { throw '-Version is required (release tag, e.g. v1.2.3).' }
if (-not (Test-Path (Join-Path $Staging 'wilfred.exe'))) { throw "Missing wilfred.exe in $Staging" }
if (-not (Test-Path (Join-Path $Staging 'ui\overlay'))) { throw "Missing ui\overlay in $Staging" }

$chocoVersion = Get-ChocoVersion $Version
Write-Host "Package version: $chocoVersion (from $Version)"

$choco = Get-Command choco.exe -ErrorAction SilentlyContinue
if (-not $choco) {
    throw ('choco.exe not found. Install Chocolatey from https://chocolatey.org/install ' +
        '(GitHub windows runners already have it).')
}

$work = Join-Path ([IO.Path]::GetTempPath()) ('wilfred-choco-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $work | Out-Null
try {
    $nuspecText = (Get-Content $Template -Raw) -replace '%%VERSION%%', $chocoVersion
    $nuspecText | Set-Content (Join-Path $work 'wilfred.nuspec') -NoNewline -Encoding UTF8
    $toolsOut = Join-Path $work 'tools'
    New-Item -ItemType Directory -Force -Path $toolsOut | Out-Null
    Copy-Item (Join-Path $Staging 'wilfred.exe') $toolsOut
    Copy-Item -Recurse (Join-Path $Staging 'ui') (Join-Path $toolsOut 'ui')
    foreach ($doc in @('README.md', 'wilfred.default.yml')) {
        $src = Join-Path $Staging $doc
        if (Test-Path $src) { Copy-Item $src $toolsOut }
    }
    Copy-Item (Join-Path $HooksDir 'chocolateyinstall.ps1') $toolsOut
    Copy-Item (Join-Path $HooksDir 'chocolateyuninstall.ps1') $toolsOut

    if (-not (Test-Path $OutDir)) {
        New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
    }
    & choco pack (Join-Path $work 'wilfred.nuspec') --outputdirectory $OutDir --no-progress
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
}

$nupkg = Join-Path $OutDir "wilfred.$chocoVersion.nupkg"
if (-not (Test-Path $nupkg)) { throw "Expected package not produced: $nupkg" }
Write-Host "Built Chocolatey package: $nupkg"
