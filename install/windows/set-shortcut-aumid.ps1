# Stamps a .lnk shortcut with an explicit System.AppUserModel.ID (AUMID).
#
# Why: docutaz.exe sets the same id at startup via
# SetCurrentProcessExplicitAppUserModelID (see src/docutaz/app/main.cpp). When a
# shortcut carries the *matching* id, the taskbar treats the pinned shortcut and
# the running window as one app — so pinning works and the icon stays stable
# across version upgrades instead of falling back to the generic blank icon.
#
# NSIS has no built-in for this shell property, so the installer calls this from
# the Install section after CreateShortCut. Best-effort: the installer ignores a
# non-zero exit so a quirk here never aborts the install.
param(
    [Parameter(Mandatory = $true)][string]$LnkPath,
    [Parameter(Mandatory = $true)][string]$AppId
)
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $LnkPath)) {
    Write-Host "set-shortcut-aumid: shortcut not found: $LnkPath"
    exit 1
}

# COM interop for IPropertyStore on the ShellLink object. Setting the
# PKEY_AppUserModel_ID property ({9F4C2855-...}, pid 5) to a VT_LPWSTR value is
# the documented way to give a shortcut an explicit AUMID.
$code = @'
using System;
using System.Runtime.InteropServices;

namespace DocutazAumid
{
    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    public struct PROPERTYKEY
    {
        public Guid fmtid;
        public uint pid;
    }

    // PROPVARIANT: WORD vt + 3 reserved WORDs (8 bytes) then the value union.
    // Offset 8 holds the pointer for VT_LPWSTR on both 32- and 64-bit.
    [StructLayout(LayoutKind.Explicit)]
    public struct PROPVARIANT
    {
        [FieldOffset(0)] public ushort vt;
        [FieldOffset(8)] public IntPtr pwszVal;
    }

    [ComImport, Guid("886D8EEB-8CF2-4446-8D02-CDBA1DBDCF99"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IPropertyStore
    {
        void GetCount(out uint cProps);
        void GetAt(uint iProp, out PROPERTYKEY pkey);
        void GetValue(ref PROPERTYKEY key, out PROPVARIANT pv);
        void SetValue(ref PROPERTYKEY key, ref PROPVARIANT pv);
        void Commit();
    }

    [ComImport, Guid("0000010b-0000-0000-C000-000000000046"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IPersistFile
    {
        void GetClassID(out Guid pClassID);
        [PreserveSig] int IsDirty();
        void Load([MarshalAs(UnmanagedType.LPWStr)] string pszFileName, int dwMode);
        void Save([MarshalAs(UnmanagedType.LPWStr)] string pszFileName,
                  [MarshalAs(UnmanagedType.Bool)] bool fRemember);
        void SaveCompleted([MarshalAs(UnmanagedType.LPWStr)] string pszFileName);
        void GetCurFile([MarshalAs(UnmanagedType.LPWStr)] out string ppszFileName);
    }

    [ComImport, Guid("00021401-0000-0000-C000-000000000046")]
    public class CShellLink { }

    public static class Setter
    {
        private const int STGM_READWRITE = 0x00000002;
        private const ushort VT_LPWSTR = 31;

        public static void Set(string lnkPath, string appId)
        {
            var key = new PROPERTYKEY
            {
                fmtid = new Guid("9F4C2855-9F79-4B39-A8D0-E1D42DE1D5F3"),
                pid = 5
            };

            object link = new CShellLink();
            try
            {
                ((IPersistFile)link).Load(lnkPath, STGM_READWRITE);

                var store = (IPropertyStore)link;
                var pv = new PROPVARIANT { vt = VT_LPWSTR, pwszVal = Marshal.StringToCoTaskMemUni(appId) };
                try
                {
                    store.SetValue(ref key, ref pv);
                    store.Commit();
                }
                finally
                {
                    Marshal.FreeCoTaskMem(pv.pwszVal);
                }

                ((IPersistFile)link).Save(lnkPath, true);
            }
            finally
            {
                Marshal.ReleaseComObject(link);
            }
        }
    }
}
'@

Add-Type -TypeDefinition $code -Language CSharp
[DocutazAumid.Setter]::Set($LnkPath, $AppId)
Write-Host "set-shortcut-aumid: stamped '$LnkPath' with AUMID '$AppId'"
