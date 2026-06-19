# Removes the Desktop and Start-menu shortcuts created by create-shortcut.ps1.
$ErrorActionPreference = 'SilentlyContinue'

# Resolve the Desktop and Start-menu (Programs) folders for the *interactive*
# user (see create-shortcut.ps1 for why the process profile isn't enough when
# run elevated under a different account).
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
        if ($interactive -ieq $current) { return $fallback }

        $sid = (New-Object System.Security.Principal.NTAccount($owner.Domain, $owner.User)).Translate(
                   [System.Security.Principal.SecurityIdentifier]).Value
        $key = "Registry::HKEY_USERS\$sid\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders"
        $sf = Get-ItemProperty -Path $key -ErrorAction Stop
        if ($sf.Desktop -and $sf.Programs) {
            return [pscustomobject]@{ Desktop = $sf.Desktop; Programs = $sf.Programs }
        }
    } catch { }
    return $fallback
}

$folders = Get-DocutazShellFolders

# Clean both the interactive user's folders and the current profile's, so a
# shortcut left behind by an older (process-profile) run is also removed.
$targets = @(
    (Join-Path $folders.Desktop  'Docutaz.lnk'),
    (Join-Path $folders.Programs 'Docutaz.lnk'),
    (Join-Path ([Environment]::GetFolderPath('Desktop'))  'Docutaz.lnk'),
    (Join-Path ([Environment]::GetFolderPath('Programs')) 'Docutaz.lnk')
) | Select-Object -Unique

foreach ($p in $targets) {
    if (Test-Path $p) {
        Remove-Item $p -Force
        Write-Host "Removed: $p"
    }
}
Write-Host "Docutaz shortcuts removed."
