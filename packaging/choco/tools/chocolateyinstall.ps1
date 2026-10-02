# Chocolatey install hook for the embedded portable Wilfred package.
# All files ship inside the nupkg tools dir (no downloads, works offline).
# Creates a per-user Start Menu shortcut; the console shim for wilfred.exe
# is generated automatically by Chocolatey.
$ErrorActionPreference = 'Stop'
$toolsDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$programs = [Environment]::GetFolderPath('Programs')
$shortcut = Join-Path $programs 'Wilfred.lnk'
Install-ChocolateyShortcut -ShortcutFilePath $shortcut `
    -TargetPath (Join-Path $toolsDir 'wilfred.exe') `
    -Description 'Wilfred - fast desktop search and launcher'
