# Creates a self-signed code-signing certificate for local Wilfred builds.
# Publisher is fixed to Wilfred Open Contributors so local test signatures
# match CI expectations.
#
# What this does and does not do:
# - DOES stamp Publisher = Wilfred Open Contributors on signatures made
#   with this cert, and lets your own machine trust it once installed.
# - DOES NOT confer SmartScreen reputation on other machines. Only a
#   Microsoft-trusted certificate (Azure Trusted Signing, or an OV/EV cert
#   from a public CA) does that. See docs/signing.md.
#
# Usage:
#   ./scripts/New-DevCodeSignCert.ps1 [-CertName 'Wilfred Open Contributors']
#                                     [-ExportPath 'build\dev-signing.pfx']
#                                     [-Password 'change-me']
#                                     [-Install]
#
# -Install adds the cert to CurrentUser Root so this machine trusts signatures
# made with it. Requires no elevation for CurrentUser. Omit to just create
# the cert in CurrentUser\My without trusting it machine-wide.
[CmdletBinding()]
param(
    [string]$CertName = 'Wilfred Open Contributors',
    [string]$ExportPath = 'build\dev-signing.pfx',
    [string]$Password = '',
    [switch]$Install
)

$ErrorActionPreference = 'Stop'

$cert = New-SelfSignedCertificate `
    -Type CodeSigningCert `
    -Subject "CN=$CertName" `
    -FriendlyName "$CertName (Wilfred local dev)" `
    -KeyUsage DigitalSignature `
    -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3') `
    -CertStoreLocation 'Cert:\CurrentUser\My' `
    -NotAfter (Get-Date).AddYears(2)

Write-Host "Created cert: $($cert.Subject) $($cert.Thumbprint)"

if ($Install) {
    $store = New-Object System.Security.Cryptography.X509Certificates.X509Store(
        'Root', 'CurrentUser')
    $store.Open('ReadWrite')
    try {
        $store.Add($cert)
        Write-Host 'Installed to CurrentUser Root. This machine now trusts it.'
    } finally {
        $store.Close()
    }
} else {
    Write-Host 'Not installed to Root. Signatures verify as Unknown publisher until installed.'
    Write-Host 'Re-run with -Install to trust on this machine only.'
}

if ($ExportPath) {
    $dir = Split-Path -Parent $ExportPath
    if ($dir -and -not (Test-Path $dir)) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
    }
    $secure = if ($Password) {
        ConvertTo-SecureString -String $Password -Force -AsPlainText
    } else {
        $chars = 'abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789'
        $rng = New-Object System.Random
        $plain = -join (1..24 | ForEach-Object { $chars[$rng.Next($chars.Length)] })
        Write-Host 'Generated random PFX password (shown once, then only in CI secrets if you use it there).'
        Write-Host $plain
        ConvertTo-SecureString -String $plain -Force -AsPlainText
    }
    Export-PfxCertificate -Cert $cert -FilePath $ExportPath -Password $secure -Force | Out-Null
    Write-Host "Exported PFX: $ExportPath"
    Write-Host "Thumbprint: $($cert.Thumbprint)"
}
