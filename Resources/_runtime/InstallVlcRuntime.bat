@echo off
setlocal enabledelayedexpansion
set "PLUGIN_ROOT=%~1"
if "%PLUGIN_ROOT%"=="" (
  echo Usage: InstallVlcRuntime.bat "Full\Path\To\Your\PluginRoot"
  exit /b 1
)

set "ZIP=%PLUGIN_ROOT%\Resources\_runtime\UnzipThis.zip"
set "DEST=%PLUGIN_ROOT%\ThirdParty\vlc\Win64"
set "TMP=%TEMP%\VlcUnzipTemp_%RANDOM%_%RANDOM%"

echo [INFO] Plugin root: "%PLUGIN_ROOT%"
echo [INFO] ZIP        : "%ZIP%"
echo [INFO] DEST       : "%DEST%"
echo [INFO] TMP        : "%TMP%"

if not exist "%ZIP%" (
  echo [ERROR] ZIP not found.
  exit /b 2
)

echo [STEP] Preparing folders...
if exist "%TMP%" rmdir /s /q "%TMP%"
mkdir "%TMP%" >nul 2>&1
mkdir "%DEST%" >nul 2>&1

echo [STEP] Extracting ZIP (PowerShell)...
powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass ^
  -Command "Expand-Archive -LiteralPath '%ZIP%' -DestinationPath '%TMP%' -Force" || (
  echo [ERROR] Expand-Archive failed.
  exit /b 3
)

rem Detect nested layout (& flatten) or direct layout
set "CAND1=%TMP%\vlc\Win64"
set "CAND2=%TMP%\UnzipThis\vlc\Win64"

if exist "%CAND1%\libvlc.dll" (
  set "WIN64SRC=%CAND1%"
) else if exist "%CAND2%\libvlc.dll" (
  set "WIN64SRC=%CAND2%"
) else (
  rem Try one more generic probe
  for /r "%TMP%" %%D in (libvlc.dll) do (
    set "WIN64SRC=%%~dpD"
  )
)

if not defined WIN64SRC (
  echo [ERROR] Could not locate Win64 folder in extracted ZIP.
  rmdir /s /q "%TMP%" >nul 2>&1
  exit /b 4
)

echo [INFO] Found Win64 folder: "%WIN64SRC%"
echo [STEP] Copying runtime files to DEST...
xcopy "%WIN64SRC%\*" "%DEST%\" /e /i /y >nul
if errorlevel 1 (
  echo [ERROR] Copy failed.
  rmdir /s /q "%TMP%" >nul 2>&1
  exit /b 5
)

echo [STEP] Cleaning up temp...
rmdir /s /q "%TMP%" >nul 2>&1

if exist "%DEST%\libvlc.dll" (
  echo [OK] VLC runtime installed.
  exit /b 0
) else (
  echo [ERROR] Copy step finished but libvlc.dll is still missing in DEST.
  exit /b 6
)
