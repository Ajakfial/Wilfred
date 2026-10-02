# Builds the Wilfred Windows installer (.msi) with WiX Toolset v3.
#
# Inputs are the same staging layout the release workflow zips:
#   <Staging>\wilfred.exe
#   <Staging>\ui\overlay\...
#   <Staging>\README.md
#   <Staging>\wilfred.default.yml
#
# Publisher is fixed to Wilfred Open Contributors (template Manufacturer).
# Version is sanitized to numeric X.Y.Z for Windows Installer.
# UpgradeCode lives in packaging/wix/Wilfred.wxs and must never change.
#
# Requires WiX Toolset v3 (candle/light/heat): preinstalled on GitHub
# windows runners, or local via: choco install wixtoolset
#
# Usage:
#   ./scripts/Build-Msi.ps1 -Staging dist\wilfred-v1.2.3-windows-x64 `
#       -Out dist\wilfred-v1.2.3-windows-x64.msi -Version v1.2.3
#   ./scripts/Build-Msi.ps1 -ValidateOnly   # no WiX needed: checks template + inputs
[CmdletBinding()]
param(
    [string]$Staging = '',
    [string]$Out = '',
    [string]$Version = '',
    [string]$UpgradeCode = '70E15177-4531-45D4-AEF7-D2E91D92F937',
    [string]$IconPath = '',
    [switch]$ValidateOnly
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Template = Join-Path $Root 'packaging\wix\Wilfred.wxs'

function Get-MsiVersion([string]$Tag) {
    $t = $Tag.Trim().TrimStart('v', 'V')
    $parts = $t -split '[.\-+_]' | Where-Object { $_ -ne '' }
    $nums = @()
    foreach ($p in $parts) {
        if ($p -match '^\d+$') { $nums += [int]$p }
        else { break }
    }
    while ($nums.Count -lt 3) { $nums += 0 }
    if ($nums[0] -gt 255 -or $nums[1] -gt 255) {
        throw "Version $Tag out of MSI range (major/minor must be 0-255)."
    }
    if ($nums[2] -gt 65535) { $nums[2] = 65535 }
    return "$($nums[0]).$($nums[1]).$($nums[2])"
}

function Find-WixTool([string]$Name) {
    $roots = @()
    if (${env:ProgramFiles(x86)}) { $roots += Join-Path ${env:ProgramFiles(x86)} 'WiX Toolset v3.14\bin' }
    if (${env:ProgramFiles(x86)}) { $roots += Join-Path ${env:ProgramFiles(x86)} 'WiX Toolset v3.11\bin' }
    if ($env:ProgramFiles) { $roots += Join-Path $env:ProgramFiles 'WiX Toolset v3.14\bin' }
    foreach ($r in $roots) {
        $exe = Join-Path $r "$Name.exe"
        if (Test-Path $exe) { return $exe }
    }
    $onPath = Get-Command "$Name.exe" -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    return $null
}

# --- Template self-checks (run even in ValidateOnly) ---
if (-not (Test-Path $Template)) { throw "WiX template not found: $Template" }
try {
    [xml]$xml = Get-Content $Template -Raw
} catch {
    throw "WiX template is not well-formed XML: $($_.Exception.Message)"
}
$ns = @{ wix = 'http://schemas.microsoft.com/wix/2006/wi' }
$product = Select-Xml -Xml $xml -XPath '/wix:Wix/wix:Product' -Namespace $ns | Select-Object -ExpandProperty Node
if (-not $product) { throw 'Template has no Product element.' }
if ($product.Manufacturer -ne 'Wilfred Open Contributors') {
    throw "Template Manufacturer is $($product.Manufacturer), expected Wilfred Open Contributors."
}
$templateCode = ($product.UpgradeCode -replace '[{}]', '').ToUpper()
if ($templateCode -ne $UpgradeCode.ToUpper()) {
    throw "Template UpgradeCode is $($product.UpgradeCode), expected $UpgradeCode. Refusing to build with a drifted upgrade code."
}
foreach ($ref in @('ProductComponents', 'OverlayFiles')) {
    $hit = Select-Xml -Xml $xml -XPath "//wix:ComponentGroupRef[@Id='$ref']" -Namespace $ns
    if (-not $hit) { throw "Template missing ComponentGroupRef $ref." }
}
foreach ($id in @('WilfredExe', 'WilfredReadme', 'WilfredDefaults', 'WilfredPath', 'WilfredMenuShortcuts')) {
    $hit = Select-Xml -Xml $xml -XPath "//wix:Component[@Id='$id']" -Namespace $ns
    if (-not $hit) { throw "Template missing Component $id." }
}
Write-Host 'Template checks passed (publisher, upgrade code, feature refs).'

if ($ValidateOnly) { exit 0 }

if (-not $Staging) { throw '-Staging is required (directory with wilfred.exe + ui/overlay + docs).' }
if (-not $Out) { throw '-Out is required (output .msi path).' }
if (-not $Version) { throw '-Version is required (release tag, e.g. v1.2.3).' }
if (-not (Test-Path (Join-Path $Staging 'wilfred.exe'))) { throw "Missing wilfred.exe in $Staging" }
if (-not (Test-Path (Join-Path $Staging 'ui\overlay'))) { throw "Missing ui\overlay in $Staging" }

$msiVersion = Get-MsiVersion $Version
Write-Host "ProductVersion: $msiVersion (from $Version)"

if (-not $IconPath) {
    $defaultIcon = Join-Path $Root 'packaging\wix\icon.ico'
    if (Test-Path $defaultIcon) { $IconPath = $defaultIcon }
}
$hasIcon = 'no'
if ($IconPath -and (Test-Path $IconPath)) { $hasIcon = 'yes' }
if ($IconPath -and $hasIcon -eq 'no') { Write-Warning "Icon not found: $IconPath; building without ARP icon." }

$candle = Find-WixTool 'candle'
$light = Find-WixTool 'light'
$heat = Find-WixTool 'heat'
if (-not $candle -or -not $light -or -not $heat) {
    $choco = Get-Command choco.exe -ErrorAction SilentlyContinue
    if ($choco) {
        Write-Host 'WiX v3 not found; attempting install via Chocolatey (needs elevation)...'
        & choco install wixtoolset --version=3.14.1 -y --no-progress
        if ($LASTEXITCODE -eq 0) {
            $candle = Find-WixTool 'candle'
            $light = Find-WixTool 'light'
            $heat = Find-WixTool 'heat'
        }
    }
}
if (-not $candle -or -not $light -or -not $heat) {
    throw ('WiX Toolset v3 (candle/light/heat) not found. ' +
        'Install with: choco install wixtoolset   (GitHub windows runners already have it).')
}
Write-Host "Using WiX: $candle"

$work = Join-Path ([IO.Path]::GetTempPath()) ("wilfred-msi-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $work | Out-Null
try {
    Copy-Item $Template (Join-Path $work 'Wilfred.wxs')
    & $heat dir (Join-Path $Staging 'ui\overlay') -dr OverlayDir -cg OverlayFiles `
        -gg -g1 -scom -sreg -sfrag -srd -var var.OverlaySource `
        -out (Join-Path $work 'overlay_files.wxs')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $defines = @(
        "-dProductVersion=$msiVersion",
        "-dSourceDir=$Staging",
        "-dOverlaySource=$(Join-Path $Staging 'ui\overlay')",
        "-dHasIcon=$hasIcon"
    )
    if ($hasIcon -eq 'yes') { $defines += "-dIconPath=$IconPath" }

    & $candle -nologo -arch x64 -ext WixUIExtension `
        (Join-Path $work 'Wilfred.wxs') (Join-Path $work 'overlay_files.wxs') `
        @defines -out ($work.TrimEnd('\') + '\')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $outDir = Split-Path -Parent $Out
    if ($outDir -and -not (Test-Path $outDir)) {
        New-Item -ItemType Directory -Force -Path $outDir | Out-Null
    }
    & $light -nologo -ext WixUIExtension `
        (Join-Path $work 'Wilfred.wixobj') (Join-Path $work 'overlay_files.wixobj') `
        -out $Out
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
}

Write-Host "Built MSI: $Out"
