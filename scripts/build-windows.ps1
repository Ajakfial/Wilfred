# Build Wilfred on Windows (CMake + MSVC).
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Config = 'Release',
    [string]$BuildDir = 'build\windows',
    [int]$Jobs = 0,
    [switch]$Tests,
    [switch]$Clean,
    [string]$Generator = '',
    [string]$Arch = 'x64',
    [switch]$Sign,
    [string]$SignThumbprint = '',
    [string]$SignPfx = '',
    [string]$SignTimestamp = 'http://timestamp.digicert.com',
    [switch]$Msi,
    [string]$MsiVersion = '',
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$CMakeArgs
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

function Find-VsDevCmd {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw 'Visual Studio Build Tools were not found (vswhere.exe missing).'
    }
    $install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $install) {
        throw 'No Visual Studio C++ toolset found. Install "Desktop development with C++".'
    }
    $devCmd = Join-Path $install 'Common7\Tools\VsDevCmd.bat'
    if (-not (Test-Path $devCmd)) {
        throw "VsDevCmd.bat not found under $install"
    }
    return $devCmd
}

function Import-VsEnvironment {
    $devCmd = Find-VsDevCmd
    $archArg = switch ($Arch) {
        'x86' { 'x86' }
        'arm64' { 'arm64' }
        default { 'x64' }
    }
    $bat = $devCmd.Replace('"', '""')
    cmd.exe /c "`"$bat`" -arch=$archArg -host_arch=x64 && set" | ForEach-Object {
        if ($_ -match '^(.*?)=(.*)$') {
            Set-Item -Path "Env:$($Matches[1])" -Value $Matches[2]
        }
    }
    if (-not $env:VCINSTALLDIR) {
        throw 'Failed to import the Visual Studio developer environment.'
    }
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw 'cmake is required (3.16+).'
}

if (-not $env:VCINSTALLDIR) {
    Import-VsEnvironment
}

if (-not $Generator) {
    if (Get-Command ninja -ErrorAction SilentlyContinue) {
        $Generator = 'Ninja'
    } else {
        $Generator = ''
    }
}

if ($Clean -and (Test-Path $BuildDir)) {
    Remove-Item -Recurse -Force $BuildDir
}

$configure = @(
    '-S', $Root,
    '-B', $BuildDir,
    "-DCMAKE_BUILD_TYPE=$Config",
    '-DWILFRED_BUILD_TESTS=ON',
    '-DWILFRED_BUILD_BENCH=ON'
)
if ($Generator) {
    $configure += @('-G', $Generator)
    if ($Generator -like 'Visual Studio*') {
        $configure += @('-A', $Arch)
    }
} else {
    $configure += @('-A', $Arch)
}
if ($CMakeArgs) {
    $configure += $CMakeArgs
}

& cmake @configure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$build = @('--build', $BuildDir, '--config', $Config)
if ($Jobs -gt 0) {
    $build += @('--parallel', "$Jobs")
} else {
    $build += '--parallel'
}
& cmake @build
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($Tests) {
    & ctest --test-dir $BuildDir --output-on-failure -C $Config
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$exeDir = Join-Path $BuildDir $Config
if (-not (Test-Path (Join-Path $exeDir 'wilfred.exe'))) {
    $exeDir = $BuildDir
}

$exe = Join-Path $exeDir 'wilfred.exe'
if ($Sign -or $SignThumbprint -or $SignPfx -or $env:WILFRED_SIGN -eq '1') {
    Write-Host ''
    Write-Host 'Signing Windows binary (Publisher: Wilfred Open Contributors)...'
    $signParams = @{ ExePath = $exe }
    if ($SignThumbprint) { $signParams.Thumbprint = $SignThumbprint }
    if ($SignPfx) { $signParams.PfxPath = $SignPfx }
    if ($SignTimestamp) { $signParams.TimestampUrl = $SignTimestamp }
    & (Join-Path $PSScriptRoot 'Sign-WindowsBinary.ps1') @signParams
} else {
    Write-Host ''
    Write-Host 'Skipping Authenticode signing (VERSIONINFO publisher stamp still applied).'
    Write-Host 'Pass -Sign to sign with your Wilfred Open Contributors dev cert,'
    Write-Host 'or set WILFRED_SIGN=1. See docs/signing.md.'
}

if ($Msi) {
    Write-Host ''
    Write-Host 'Building Windows installer (.msi)...'
    $msiStaging = Join-Path ([IO.Path]::GetTempPath()) ('wilfred-msi-staging-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force -Path $msiStaging | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $msiStaging 'ui') | Out-Null
    try {
        Copy-Item $exe $msiStaging
        Copy-Item -Recurse (Join-Path $Root 'ui\overlay') (Join-Path $msiStaging 'ui\overlay')
        Copy-Item (Join-Path $Root 'README.md') $msiStaging -ErrorAction SilentlyContinue
        Copy-Item (Join-Path $Root 'config\wilfred.default.yml') $msiStaging -ErrorAction SilentlyContinue
        if ($MsiVersion) {
            $msiTag = $MsiVersion
        } else {
            $projLine = Get-Content (Join-Path $Root 'CMakeLists.txt') |
                Select-String -Pattern 'project\(Wilfred VERSION ([0-9.]+)' |
                Select-Object -First 1
            $msiTag = 'v' + $projLine.Matches[0].Groups[1].Value
        }
        $msiOut = Join-Path $Root "dist\wilfred-$msiTag-windows-x64.msi"
        $msiParams = @{ Staging = $msiStaging; Out = $msiOut; Version = $msiTag }
        & (Join-Path $PSScriptRoot 'Build-Msi.ps1') @msiParams
        if ($Sign -or $SignThumbprint -or $SignPfx -or $env:WILFRED_SIGN -eq '1') {
            Write-Host 'Signing installer...'
            $signParams.ExePath = $msiOut
            & (Join-Path $PSScriptRoot 'Sign-WindowsBinary.ps1') @signParams
        }
        Write-Host "Installer: $msiOut"
    } finally {
        Remove-Item -Recurse -Force $msiStaging -ErrorAction SilentlyContinue
    }
}

Write-Host ""
Write-Host "Built:"
Write-Host "  $(Join-Path $exeDir 'wilfred.exe')"
Write-Host "  $(Join-Path $exeDir 'wilfred_tests.exe')"
Write-Host "  $(Join-Path $exeDir 'wilfred_bench.exe')"
