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

Write-Host ""
Write-Host "Built:"
Write-Host "  $(Join-Path $exeDir 'wilfred.exe')"
Write-Host "  $(Join-Path $exeDir 'wilfred_tests.exe')"
Write-Host "  $(Join-Path $exeDir 'wilfred_bench.exe')"
