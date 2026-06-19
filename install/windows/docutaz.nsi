; Docutaz Windows installer (NSIS / Nullsoft).
;
; This is a *standalone* script: it packages the fully-assembled, smoke-tested
; portable directory that the CI Windows job already produces for the zip
; (windeployqt'd docutaz.exe + every runtime DLL + platform plugins). We do NOT
; drive this through CPack — the portable dir is already complete and verified,
; so pointing makensis at it is simpler and less fragile than re-deriving the
; file set from install() rules.
;
; Invoked from the repo root by CI as:
;   makensis /DVERSION=2.1.0 \
;            /DINSTALL_SRC=docutaz-2.1.0-windows-x86_64 \
;            /DOUTFILE=docutaz-2.1.0-windows-x86_64-setup.exe \
;            install\windows\docutaz.nsi
;
; Produces a per-machine installer (Program Files) with Start-menu + Desktop
; shortcuts, an Add/Remove-Programs entry, and a matching uninstaller. Supports
; silent install/uninstall via /S — required by Winget (InstallerType: nullsoft).

Unicode true

!ifndef VERSION
  !define VERSION "0.0.0"
!endif
!ifndef INSTALL_SRC
  !error "INSTALL_SRC (the assembled portable dir) must be passed with /DINSTALL_SRC=..."
!endif
!ifndef OUTFILE
  !define OUTFILE "docutaz-${VERSION}-windows-x86_64-setup.exe"
!endif

!define APP_NAME      "Docutaz"
!define PUBLISHER     "Illimitable Consulting Private Limited"
!define EXE_NAME      "docutaz.exe"
; Must match SetCurrentProcessExplicitAppUserModelID() in src/docutaz/app/main.cpp.
!define AUMID         "Illimitable.Docutaz"
!define ABOUT_URL     "https://illimitable-consulting-private-limited.github.io/docutaz/"
!define UNINST_KEY    "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"

; NSIS resolves icon/bitmap/license paths relative to THIS script's directory,
; so the co-located assets are referenced by bare name and LICENSE as ..\..\.
; (INSTALL_SRC/OUTFILE are passed as absolute paths from CI instead.)

Name "${APP_NAME} ${VERSION}"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\${APP_NAME}"
; Reuse the previous install location on upgrade, if any.
InstallDirRegKey HKLM "${UNINST_KEY}" "InstallLocation"
RequestExecutionLevel admin           ; per-machine install needs elevation
SetCompressor /SOLID lzma
ShowInstDetails show
ShowUnInstDetails show

!include "MUI2.nsh"
!include "FileFunc.nsh"

; ---- Modern UI look ----
!define MUI_ICON   "docutaz.ico"
!define MUI_UNICON "docutaz.ico"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "nsis-topbar.bmp"
!define MUI_WELCOMEFINISHPAGE_BITMAP "nsis-sidebar.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "nsis-sidebar.bmp"
!define MUI_ABORTWARNING

; Offer to launch the app from the finish page.
!define MUI_FINISHPAGE_RUN "$INSTDIR\${EXE_NAME}"
!define MUI_FINISHPAGE_RUN_TEXT "Launch ${APP_NAME}"

; ---- Pages ----
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; The installer itself is a 32-bit exe; without this it would write the
; Add/Remove-Programs entry into the WOW6432Node redirect rather than the native
; 64-bit hive where Windows/Winget look for this 64-bit app. Set it before the
; automatic InstallDirRegKey lookup runs (end of .onInit).
Function .onInit
  SetRegView 64
FunctionEnd

; ---------------------------------------------------------------------------
Section "Install"
  SetRegView 64
  SetOutPath "$INSTDIR"

  ; Clean any previous install in-place first so renamed/removed DLLs across
  ; versions don't linger (Winget upgrades run the new installer over the old).
  ReadRegStr $0 HKLM "${UNINST_KEY}" "QuietUninstallString"
  ${If} $0 != ""
    DetailPrint "Removing previous ${APP_NAME} installation..."
    ; _?=$INSTDIR keeps the uninstaller from copying itself to %TEMP%, so the
    ; call is synchronous and we can install over it immediately afterwards.
    ExecWait '$0 _?=$INSTDIR'
  ${EndIf}

  ; Pull in the entire assembled portable directory (exe, all DLLs, plugins).
  File /r "${INSTALL_SRC}\*"

  ; Start-menu + Desktop shortcuts.
  CreateShortCut "$SMPROGRAMS\${APP_NAME}.lnk" "$INSTDIR\${EXE_NAME}"
  CreateShortCut "$DESKTOP\${APP_NAME}.lnk"    "$INSTDIR\${EXE_NAME}"

  ; Stamp both shortcuts with the same AppUserModelID the running process sets,
  ; so the taskbar matches the pinned shortcut to the live window and keeps the
  ; icon stable across upgrades. NSIS has no built-in for this shell property, so
  ; a small bundled PowerShell helper sets it. Extracted to the temp plugins dir
  ; (auto-cleaned) and run best-effort — a failure must not abort the install.
  InitPluginsDir
  File "/oname=$PLUGINSDIR\set-shortcut-aumid.ps1" "set-shortcut-aumid.ps1"
  nsExec::ExecToLog 'powershell -NoProfile -ExecutionPolicy Bypass -File "$PLUGINSDIR\set-shortcut-aumid.ps1" -LnkPath "$SMPROGRAMS\${APP_NAME}.lnk" -AppId "${AUMID}"'
  Pop $0
  nsExec::ExecToLog 'powershell -NoProfile -ExecutionPolicy Bypass -File "$PLUGINSDIR\set-shortcut-aumid.ps1" -LnkPath "$DESKTOP\${APP_NAME}.lnk" -AppId "${AUMID}"'
  Pop $0

  ; Uninstaller + Add/Remove Programs registration.
  WriteUninstaller "$INSTDIR\uninstall.exe"
  WriteRegStr   HKLM "${UNINST_KEY}" "DisplayName"     "${APP_NAME}"
  WriteRegStr   HKLM "${UNINST_KEY}" "DisplayVersion"  "${VERSION}"
  WriteRegStr   HKLM "${UNINST_KEY}" "Publisher"       "${PUBLISHER}"
  WriteRegStr   HKLM "${UNINST_KEY}" "DisplayIcon"     "$INSTDIR\${EXE_NAME}"
  WriteRegStr   HKLM "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr   HKLM "${UNINST_KEY}" "URLInfoAbout"    "${ABOUT_URL}"
  WriteRegStr   HKLM "${UNINST_KEY}" "UninstallString"      '"$INSTDIR\uninstall.exe"'
  WriteRegStr   HKLM "${UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1

  ; Report the on-disk size to Add/Remove Programs (in KB).
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKLM "${UNINST_KEY}" "EstimatedSize" $0
SectionEnd

; ---------------------------------------------------------------------------
Function un.onInit
  SetRegView 64
FunctionEnd

Section "Uninstall"
  SetRegView 64
  Delete "$SMPROGRAMS\${APP_NAME}.lnk"
  Delete "$DESKTOP\${APP_NAME}.lnk"

  ; Remove everything we installed. RMDir /r on the install dir is safe because
  ; we install into a dedicated per-app directory under Program Files.
  RMDir /r "$INSTDIR"

  DeleteRegKey HKLM "${UNINST_KEY}"
SectionEnd
