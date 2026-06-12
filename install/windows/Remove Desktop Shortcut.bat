@echo off
rem Double-click to remove the Docutaz Desktop and Start-menu shortcuts.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0remove-shortcut.ps1"
echo.
pause
