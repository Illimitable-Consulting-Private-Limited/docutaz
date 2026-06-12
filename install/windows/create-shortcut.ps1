# Creates Desktop and Start-menu shortcuts to docutaz.exe (in this folder).
# Per-user only — no administrator rights needed. Invoked by
# "Create Desktop Shortcut.bat"; can also be run directly.
$ErrorActionPreference = 'Stop'

$dir = $PSScriptRoot
$exe = Join-Path $dir 'docutaz.exe'
if (-not (Test-Path $exe)) {
    Write-Host "ERROR: docutaz.exe was not found next to this script:" -ForegroundColor Red
    Write-Host "       $dir"
    Write-Host "Run this from the extracted Docutaz folder."
    exit 1
}

$shell = New-Object -ComObject WScript.Shell
function New-DocutazShortcut([string]$path) {
    $lnk = $shell.CreateShortcut($path)
    $lnk.TargetPath       = $exe
    $lnk.WorkingDirectory = $dir
    $lnk.IconLocation     = "$exe,0"   # the exe's embedded icon
    $lnk.Description       = 'Docutaz - MongoDB GUI'
    $lnk.Save()
    Write-Host "Created: $path"
}

New-DocutazShortcut (Join-Path ([Environment]::GetFolderPath('Desktop'))  'Docutaz.lnk')
New-DocutazShortcut (Join-Path ([Environment]::GetFolderPath('Programs')) 'Docutaz.lnk')

Write-Host ""
Write-Host "Done - Docutaz is now on your Desktop and in the Start menu." -ForegroundColor Green
