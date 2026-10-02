# Signs a Windows binary with Authenticode so Publisher shows as
# Wilfred Open Contributors instead of Unknown.
#
# Signing modes (first match wins):
#  1. Azure Trusted Signing (recommended for releases): pass -Azure* args or
#     set AZURE_TENANT_ID / AZURE_CLIENT_ID / AZURE_CLIENT_SECRET /
#     AZURE_TRUSTED_SIGNING_ACCOUNT / AZURE_TRUSTED_SIGNING_PROFILE /
#     AZURE_TRUSTED_SIGNING_ENDPOINT env vars. Uses AzureSignTool if present,
#     else falls back to signtool with RFC3161 timestamping where supported.
#  2. PFX file: -PfxPath (with -PfxPassword env or param). This is how
#     self-signed local certs from New-DevCodeSignCert.ps1 are used, or a real
#     OV/EV PFX in CI via WILFRED_PFX_BASE64 / WILFRED_PFX_PASSWORD secrets.
#  3. Installed cert: -Thumbprint of a cert in Cert:\CurrentUser\My.
#
# What signing does and does not do:
# - DOES set the Authenticode Publisher shown in Explorer, UAC, and
#   Get-AuthenticodeSignature to the cert subject (CN=Wilfred Open Contributors
#   for dev certs, or your trusted cert subject in CI).
# - DOES NOT grant SmartScreen reputation by itself when self-signed. Only a
#   Microsoft-trusted signature (Azure Trusted Signing or public OV/EV cert)
#   plus built-up reputation silences SmartScreen on other machines.
#   See docs/signing.md.
#
# Usage:
#   ./scripts/Sign-WindowsBinary.ps1 -ExePath build\windows\Release\wilfred.exe
#   ./scripts/Sign-WindowsBinary.ps1 -ExePath wilfred.exe -Thumbprint ABC123...
#   ./scripts/Sign-WindowsBinary.ps1 -ExePath wilfred.exe -PfxPath build\dev-signing.pfx
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExePath,
    [string]$Thumbprint = '',
    [string]$PfxPath = '',
    [string]$PfxPassword = $env:WILFRED_PFX_PASSWORD,
    [string]$TimestampUrl = 'http://timestamp.digicert.com',
    [string]$AzureTenantId = $env:AZURE_TENANT_ID,
    [string]$AzureClientId = $env:AZURE_CLIENT_ID,
    [string]$AzureClientSecret = $env:AZURE_CLIENT_SECRET,
    [string]$AzureAccount = $env:AZURE_TRUSTED_SIGNING_ACCOUNT,
    [string]$AzureProfile = $env:AZURE_TRUSTED_SIGNING_PROFILE,
    [string]$AzureEndpoint = $env:AZURE_TRUSTED_SIGNING_ENDPOINT,
    [switch]$VerifyOnly
)

$ErrorActionPreference = 'Stop'

function Find-SignTool {
    $candidates = @()
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        try {
            $kits = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
        } catch { $kits = $null }
    }
    $roots = @()
    if (${env:ProgramFiles(x86)}) { $roots += Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin' }
    if ($env:ProgramFiles) { $roots += Join-Path $env:ProgramFiles 'Windows Kits\10\bin' }
    foreach ($root in $roots) {
        if (-not (Test-Path $root)) { continue }
        $found = Get-ChildItem -Path $root -Recurse -Filter 'signtool.exe' -ErrorAction SilentlyContinue
        if (-not $found) { continue }
        $best = $found | Where-Object { $_.FullName -like '*\x64\*' } | Select-Object -First 1
        if (-not $best) { $best = $found | Sort-Object FullName | Select-Object -First 1 }
        if ($best) { $candidates += $best.FullName }
    }
    $onPath = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if ($onPath) { $candidates += $onPath.Source }
    foreach ($c in $candidates) {
        if ($c -and (Test-Path $c)) { return $c }
    }
    throw 'signtool.exe not found. Install Windows SDK or Visual Studio Build Tools.'
}

if (-not (Test-Path $ExePath)) {
    throw "Binary not found: $ExePath"
}

if ($VerifyOnly) {
    $sig = Get-AuthenticodeSignature -FilePath $ExePath
    $sig | Format-List Status, StatusMessage, SignerCertificate, TimeStamperCertificate, Path
    if ($sig.Status -ne 'Valid') { exit 1 }
    exit 0
}

# 1. Azure Trusted Signing when fully configured.
if ($AzureTenantId -and $AzureClientId -and $AzureClientSecret -and ($AzureEndpoint -or ($AzureAccount -and $AzureProfile))) {
    $azTool = Get-Command AzureSignTool -ErrorAction SilentlyContinue
    if (-not $azTool) {
        Write-Host 'Azure Trusted Signing configured but AzureSignTool not found; installing via dotnet tool (user-local)...'
        & dotnet tool install --global AzureSignTool 2>$null
        $azTool = Get-Command AzureSignTool -ErrorAction SilentlyContinue
    }
    if ($azTool) {
        $args = @('sign', '-kvu', $AzureEndpoint, '-kvi', $AzureClientId, '-kvt', $AzureTenantId,
            '-kvs', $AzureClientSecret, '-kvc', $AzureAccount, '-kvp', $AzureProfile,
            '-tr', 'http://timestamp.acs.microsoft.com', '-v', $ExePath)
        # When only an endpoint is supplied (newer AzureSignTool), prefer it.
        if ($AzureEndpoint -and -not $AzureAccount) {
            $args = @('sign', '-kvu', $AzureEndpoint, '-kvi', $AzureClientId, '-kvt', $AzureTenantId,
                '-kvs', $AzureClientSecret, '-tr', 'http://timestamp.acs.microsoft.com', '-v', $ExePath)
        }
        & AzureSignTool @args
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & $PSScriptRoot/Sign-WindowsBinary.ps1 -ExePath $ExePath -VerifyOnly
        exit 0
    }
    Write-Warning 'Azure env present but AzureSignTool unavailable; falling back to signtool PFX/thumbprint modes.'
}

$signtool = Find-SignTool
Write-Host "Using signtool: $signtool"

$signArgs = @('sign', '/fd', 'SHA256')
if ($TimestampUrl) { $signArgs += @('/tr', $TimestampUrl, '/td', 'SHA256') }

if ($PfxPath) {
    if (-not (Test-Path $PfxPath)) { throw "PFX not found: $PfxPath" }
    $signArgs += @('/f', $PfxPath)
    if ($PfxPassword) {
        $signArgs += @('/p', $PfxPassword)
    } else {
        Write-Warning 'No PFX password supplied; trying without one (may prompt or fail).'
    }
} elseif ($Thumbprint) {
    $norm = $Thumbprint -replace '[^0-9a-fA-F]', ''
    $signArgs += @('/sha1', $norm)
} else {
    # Default local path: pick the newest Wilfred Open Contributors cert.
    $cert = Get-ChildItem 'Cert:\CurrentUser\My' -CodeSigningCert -ErrorAction SilentlyContinue |
        Where-Object { $_.Subject -like '*Wilfred Open Contributors*' } |
        Sort-Object NotAfter -Descending | Select-Object -First 1
    if (-not $cert) {
        throw ('No signing identity found. Create one with scripts/New-DevCodeSignCert.ps1, ' +
            'or pass -Thumbprint / -PfxPath, or configure Azure Trusted Signing env vars.')
    }
    Write-Host "Using installed cert: $($cert.Subject) $($cert.Thumbprint)"
    $signArgs += @('/sha1', $cert.Thumbprint)
}

$signArgs += @($ExePath)
& $signtool @signArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$sig = Get-AuthenticodeSignature -FilePath $ExePath
$sig | Format-List Status, StatusMessage, SignerCertificate, Path
if ($sig.Status -ne 'Valid' -and $sig.Status -ne 'UnknownError') {
    Write-Warning "Signature status is $($sig.Status): $($sig.StatusMessage)"
    Write-Warning 'Self-signed signatures show Valid only on machines trusting the cert. This is expected locally.'
}
