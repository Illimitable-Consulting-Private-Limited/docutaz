# Removes the Desktop and Start-menu shortcuts created by create-shortcut.ps1.
$ErrorActionPreference = 'SilentlyContinue'

$targets = @(
    (Join-Path ([Environment]::GetFolderPath('Desktop'))  'Docutaz.lnk'),
    (Join-Path ([Environment]::GetFolderPath('Programs')) 'Docutaz.lnk')
)
foreach ($p in $targets) {
    if (Test-Path $p) {
        Remove-Item $p -Force
        Write-Host "Removed: $p"
    }
}
Write-Host "Docutaz shortcuts removed."
