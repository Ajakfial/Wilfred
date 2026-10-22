# Builds wilfred-setup.exe: the TypeScript setup wizard (installer/ui) plus the
# Win32/WebView2 host (installer/host), with the wizard HTML and the MSI baked
# in as resources.
#
# The setup.exe is self-contained: it extracts the embedded MSI to %TEMP% and
# drives it with MsiInstallProductW, so it can be shipped next to the plain
# .msi (or on its own) without requiring the two files to stay together.
#
# Requires:
#   - Node.js/npm (builds the wizard, same toolchain as ui/overlay)
#   - Visual Studio C++ tools + CMake (builds the host)
#   - Internet on first run only, to fetch the WebView2 SDK from NuGet
#
# Usage:
#   ./scripts/Build-Setup.ps1 -Msi dist\wilfred-v1.2.3-windows-x64.msi
#   ./scripts/Build-Setup.ps1 -Msi ... -Out dist\wilfred-setup.exe -UiCheck
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Msi,
    [string]$Version = '',
    [string]$Out = '',
    [string]$WebView2Version = '1.0.2903.40',
    [string]$BuildDir = 'build\setup-host',
    [switch]$SkipUiBuild,
    [switch]$UiCheck,
    [switch]$KeepIntermediate
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$UiDir = Join-Path $Root 'installer\ui'
$HostDir = Join-Path $Root 'installer\host'

# --- Inputs ---------------------------------------------------------------

if (-not (Test-Path $Msi)) { throw "MSI not found: $Msi" }
$MsiPath = (Resolve-Path $Msi).Path

if (-not $Version) {
    $name = [IO.Path]::GetFileNameWithoutExtension($MsiPath)
    if ($name -match 'v?(\d+\.\d+\.\d+)') {
        $Version = $Matches[1]
    } else {
        throw "Could not derive a version from '$name'. Pass -Version vX.Y.Z."
    }
}
$Version = $Version.Trim().TrimStart('v', 'V')
if ($Version -notmatch '^\d+\.\d+\.\d+$') {
    throw "Version must be numeric X.Y.Z (got '$Version')."
}

$parts = @($Version -split '\.' | ForEach-Object { [int]$_ })
while ($parts.Count -lt 4) { $parts += 0 }
for ($i = 0; $i -lt 4; $i++) {
    if ($parts[$i] -gt 65535) { $parts[$i] = 65535 }
}
$versionComma = $parts -join ','

if (-not $Out) {
    $Out = Join-Path (Split-Path -Parent $MsiPath) `
        ([IO.Path]::GetFileNameWithoutExtension($MsiPath) + '-setup.exe')
}
$OutDir = Split-Path -Parent $Out
if ($OutDir -and -not (Test-Path $OutDir)) {
    New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
}
$Out = [IO.Path]::GetFullPath($Out)

$IconPath = Join-Path $Root 'appicon\appicon.ico'
if (-not (Test-Path $IconPath)) { throw "Icon not found: $IconPath" }

Write-Host "Setup version: $Version (from $([IO.Path]::GetFileName($MsiPath)))"

# --- Wizard UI ------------------------------------------------------------

if (-not $SkipUiBuild) {
    $npm = Get-Command npm.cmd -ErrorAction SilentlyContinue
    if (-not $npm) { $npm = Get-Command npm -ErrorAction SilentlyContinue }
    if (-not $npm) {
        throw 'npm was not found. Install Node.js (the release workflow already has it).'
    }
    if (-not (Test-Path (Join-Path $UiDir 'package-lock.json'))) {
        throw 'installer/ui/package-lock.json is missing; run npm install in installer/ui and commit the lockfile.'
    }

    Push-Location $UiDir
    try {
        if (-not (Test-Path (Join-Path $UiDir 'node_modules'))) {
            Write-Host 'Installing wizard dependencies...'
            & $npm.Source ci --no-audit --no-fund --loglevel=error
            if ($LASTEXITCODE -ne 0) { throw "npm ci failed ($LASTEXITCODE)." }
        }
        Write-Host 'Building wizard UI...'
        $env:WILFRED_SETUP_VERSION = $Version
        & $npm.Source run build
        if ($LASTEXITCODE -ne 0) { throw "npm run build failed ($LASTEXITCODE)." }
        & $npm.Source run typecheck
        if ($LASTEXITCODE -ne 0) { throw "wizard typecheck failed ($LASTEXITCODE)." }
    } finally {
        Remove-Item Env:\WILFRED_SETUP_VERSION -ErrorAction SilentlyContinue
        Pop-Location
    }
}

$htmlPath = Join-Path $UiDir 'dist\index.html'
if (-not (Test-Path $htmlPath)) {
    throw "Wizard bundle not found: $htmlPath (run without -SkipUiBuild)."
}
$htmlBytes = (Get-Item $htmlPath).Length
if ($htmlBytes -gt 2MB) {
    throw "Wizard bundle is $htmlBytes bytes; WebView2 NavigateToString caps at 2 MB."
}
Write-Host "Wizard bundle: $htmlPath ($([math]::Round($htmlBytes / 1024, 1)) KiB)"

# --- WebView2 SDK ---------------------------------------------------------

$sdkRoot = Join-Path $Root "build\webview2\$WebView2Version"
$sdkHeader = Join-Path $sdkRoot 'build\native\include\WebView2.h'
$sdkLoader = Join-Path $sdkRoot 'build\native\x64\WebView2LoaderStatic.lib'
if (-not (Test-Path $sdkHeader) -or -not (Test-Path $sdkLoader)) {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $zip = Join-Path $env:TEMP "microsoft.web.webview2.$WebView2Version.zip"
    Write-Host "Downloading WebView2 SDK $WebView2Version..."
    Invoke-WebRequest -Uri "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/$WebView2Version" `
        -OutFile $zip -UseBasicParsing
    if (Test-Path $sdkRoot) { Remove-Item $sdkRoot -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $sdkRoot | Out-Null
    Expand-Archive -Path $zip -DestinationPath $sdkRoot -Force
    Remove-Item $zip -Force -ErrorAction SilentlyContinue
}
if (-not (Test-Path $sdkHeader) -or -not (Test-Path $sdkLoader)) {
    throw "WebView2 SDK is incomplete under $sdkRoot."
}
Write-Host "WebView2 SDK: $sdkRoot"

# --- MSVC environment -----------------------------------------------------

function Find-VsDevCmd {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw 'Visual Studio Build Tools were not found (vswhere.exe missing).'
    }
    $install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $install) { throw 'No Visual Studio C++ toolset found.' }
    $devCmd = Join-Path $install 'Common7\Tools\VsDevCmd.bat'
    if (-not (Test-Path $devCmd)) { throw "VsDevCmd.bat not found under $install" }
    return $devCmd
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { throw 'cmake is required (3.20+).' }

if (-not $env:VCINSTALLDIR) {
    $devCmd = (Find-VsDevCmd).Replace('"', '""')
    cmd.exe /c "`"$devCmd`" -arch=x64 -host_arch=x64 && set" | ForEach-Object {
        if ($_ -match '^(.*?)=(.*)$') {
            Set-Item -Path "Env:$($Matches[1])" -Value $Matches[2]
        }
    }
    if (-not $env:VCINSTALLDIR) { throw 'Failed to import the Visual Studio developer environment.' }
}

# --- Host -----------------------------------------------------------------

if (-not $KeepIntermediate -and (Test-Path $BuildDir)) {
    Remove-Item -Recurse -Force $BuildDir
}

$generator = @()
if (Get-Command ninja -ErrorAction SilentlyContinue) {
    $generator = @('-G', 'Ninja')
} else {
    $generator = @('-A', 'x64')
}

$configure = @(
    '-S', $HostDir,
    '-B', $BuildDir,
    "-DWEBVIEW2_ROOT=$sdkRoot",
    "-DSETUP_MSI=$MsiPath",
    "-DSETUP_HTML=$htmlPath",
    "-DSETUP_ICON=$IconPath",
    "-DSETUP_VERSION=$Version",
    "-DSETUP_VERSION_COMMA=$versionComma",
    '-DCMAKE_BUILD_TYPE=Release'
) + $generator

Write-Host 'Configuring setup host...'
& cmake @configure
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed ($LASTEXITCODE)." }

Write-Host 'Building setup host...'
& cmake --build $BuildDir --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "cmake build failed ($LASTEXITCODE)." }

$built = Get-ChildItem -Path $BuildDir -Recurse -Filter 'wilfred-setup.exe' |
    Where-Object { $_.FullName -notmatch 'CMakeFiles' } |
    Select-Object -First 1
if (-not $built) { throw "wilfred-setup.exe not found under $BuildDir." }

Copy-Item $built.FullName $Out -Force
Write-Host "Built setup: $Out ($([math]::Round((Get-Item $Out).Length / 1MB, 1)) MiB)"

# --- Self-checks ----------------------------------------------------------

$check = Start-Process -FilePath $Out -ArgumentList '--check' -Wait -PassThru -NoNewWindow
if ($check.ExitCode -ne 0) {
    throw "Embedded payload check failed (exit $($check.ExitCode))."
}

if ($UiCheck) {
    Write-Host 'Checking that the wizard loads in WebView2...'
    $ui = Start-Process -FilePath $Out -ArgumentList '--ui-check' -Wait -PassThru
    if ($ui.ExitCode -ne 0) {
        throw "Wizard UI check failed (exit $($ui.ExitCode)). WebView2 Runtime missing?"
    }
    Write-Host 'Wizard UI check passed.'
}
