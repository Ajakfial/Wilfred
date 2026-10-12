# One-command build + test for Windows (PowerShell 5.1+).
# Usage: powershell -ExecutionPolicy Bypass -File scripts/build-windows.ps1 [-Config Release]
param([string]$Config = "Release")

$ErrorActionPreference = "Stop"
$preset = "windows-vs2022"

function Need($cmd, $hint) {
  if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) {
    Write-Error "error: $cmd not found ($hint)"
    exit 1
  }
}
Need cmake "install from https://cmake.org/download/ or winget install Kitware.CMake"

Write-Host "==> configure ($preset)"
cmake --preset $preset
Write-Host "==> build ($Config)"
cmake --build --preset $preset --config $Config --parallel
Write-Host "==> test"
ctest --test-dir "build/$preset" -C $Config --output-on-failure
Write-Host "==> info"
& "build/$preset/$Config/spoke_cli.exe" info
Write-Host "OK: Windows build green. Library: build/$preset/$Config/spoke.dll"
