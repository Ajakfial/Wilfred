# Repairs a WiX heat fragment for per-user installs (ICE38/ICE64).
# Heat generates file-keypath components with no RemoveFolders; components
# under the user profile must instead use HKCU registry keypaths, and every
# profile directory needs a RemoveFolder entry. This fixes the fragment in
# place. Dot-sourceable with no side effects (used by Build-Msi.ps1 and tests).
function Repair-HeatFragment([string]$Path) {
    $WixNs = 'http://schemas.microsoft.com/wix/2006/wi'
    try {
        [xml]$frag = Get-Content $Path -Raw
    } catch {
        throw "Heat fragment is not well-formed XML: $($_.Exception.Message)"
    }
    $ns = @{ wix = $WixNs }
    $comps = @($frag | Select-Xml -XPath '//wix:Component' -Namespace $ns | ForEach-Object { $_.Node })
    if ($comps.Count -eq 0) { throw "Heat fragment has no components: $Path" }
    foreach ($comp in $comps) {
        $cid = $comp.GetAttribute('Id')
        foreach ($f in @($comp | Select-Xml -XPath 'wix:File' -Namespace $ns | ForEach-Object { $_.Node })) {
            $f.SetAttribute('KeyPath', 'no')
        }
        $reg = $frag.CreateElement('RegistryValue', $WixNs)
        $reg.SetAttribute('Root', 'HKCU')
        $reg.SetAttribute('Key', 'Software\Wilfred Open Contributors\Wilfred\Overlay')
        $reg.SetAttribute('Name', $cid)
        $reg.SetAttribute('Type', 'integer')
        $reg.SetAttribute('Value', '1')
        $reg.SetAttribute('KeyPath', 'yes')
        [void]$comp.AppendChild($reg)
    }
    $first = $comps[0]
    $dirs = @($frag | Select-Xml -XPath '//wix:Directory' -Namespace $ns | ForEach-Object { $_.Node })
    foreach ($d in $dirs) {
        $did = $d.GetAttribute('Id')
        if (-not $did) { continue }
        $rm = $frag.CreateElement('RemoveFolder', $WixNs)
        $rm.SetAttribute('Id', "Rm$did")
        $rm.SetAttribute('Directory', $did)
        $rm.SetAttribute('On', 'uninstall')
        [void]$first.AppendChild($rm)
    }
    $frag.Save($Path)
    Write-Host "Repaired heat fragment: $($comps.Count) registry keypaths, $($dirs.Count) RemoveFolders."
}
