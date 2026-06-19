# Creates Desktop and Start-menu shortcuts to docutaz.exe (in this folder).
# No administrator rights are needed. If you DO run it elevated (right click ->
# Run as administrator) under a separate admin account, it still targets the
# logged-in user's Desktop / Start menu, not the admin profile's.
# Invoked by "Create Desktop Shortcut.bat"; can also be run directly.
$ErrorActionPreference = 'Stop'

$dir = $PSScriptRoot
$exe = Join-Path $dir 'docutaz.exe'
if (-not (Test-Path $exe)) {
    Write-Host "ERROR: docutaz.exe was not found next to this script:" -ForegroundColor Red
    Write-Host "       $dir"
    Write-Host "Run this from the extracted Docutaz folder."
    exit 1
}

# Resolve the Desktop and Start-menu (Programs) folders for the *interactive*
# user. When this runs elevated under a different account, the process profile is
# the admin's, so [Environment]::GetFolderPath would drop the shortcuts on the
# admin's Desktop. In that case we read the console user's own shell folders from
# their registry hive instead.
function Get-DocutazShellFolders {
    $fallback = [pscustomobject]@{
        Desktop  = [Environment]::GetFolderPath('Desktop')
        Programs = [Environment]::GetFolderPath('Programs')
    }
    try {
        if (-not ('Docutaz.ConsoleSession' -as [type])) {
            Add-Type -Namespace Docutaz -Name ConsoleSession -MemberDefinition @'
[System.Runtime.InteropServices.DllImport("kernel32.dll")]
public static extern uint WTSGetActiveConsoleSessionId();
'@
        }
        $session = [Docutaz.ConsoleSession]::WTSGetActiveConsoleSessionId()

        $explorer = Get-CimInstance Win32_Process -Filter "Name='explorer.exe'" -ErrorAction Stop |
            Where-Object { $_.SessionId -eq $session } | Select-Object -First 1
        if (-not $explorer) { return $fallback }

        $owner = Invoke-CimMethod -InputObject $explorer -MethodName GetOwner -ErrorAction Stop
        if ($owner.ReturnValue -ne 0 -or -not $owner.User) { return $fallback }

        $interactive = "$($owner.Domain)\$($owner.User)"
        $current = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name
        if ($interactive -ieq $current) { return $fallback }   # same user: process folders are correct

        $sid = (New-Object System.Security.Principal.NTAccount($owner.Domain, $owner.User)).Translate(
                   [System.Security.Principal.SecurityIdentifier]).Value
        $key = "Registry::HKEY_USERS\$sid\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders"
        $sf = Get-ItemProperty -Path $key -ErrorAction Stop
        if ($sf.Desktop -and $sf.Programs) {
            Write-Host "Detected logged-in user '$interactive' (script is running as '$current'); using their folders."
            return [pscustomobject]@{ Desktop = $sf.Desktop; Programs = $sf.Programs }
        }
    } catch {
        Write-Host ("Note: could not resolve the logged-in user's folders ({0}); using the current profile." -f $_.Exception.Message) -ForegroundColor Yellow
    }
    return $fallback
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

$folders = Get-DocutazShellFolders
New-DocutazShortcut (Join-Path $folders.Desktop  'Docutaz.lnk')
New-DocutazShortcut (Join-Path $folders.Programs 'Docutaz.lnk')

Write-Host ""
Write-Host "Done - Docutaz is now on your Desktop and in the Start menu." -ForegroundColor Green
