# Chocolatey uninstall hook for Wilfred. Removes the Start Menu shortcut;
# the package files and shim are removed automatically by Chocolatey.
$ErrorActionPreference = 'Stop'
$shortcut = Join-Path ([Environment]::GetFolderPath('Programs')) 'Wilfred.lnk'
Remove-Item $shortcut -Force -ErrorAction SilentlyContinue
