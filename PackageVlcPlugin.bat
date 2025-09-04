@echo off
setlocal EnableExtensions

rem ================================================================================
rem                               CONFIG (EDIT)
rem ================================================================================
set "UE_DIR=D:\Program Files\Epic Games\UE_5.4"
set "PLUGIN_UPLUGIN=E:\Unreal Projects\5.4\VlcMedia_UE_Packaged\Plugins\VlcMedia\VlcMedia.uplugin"
set "PACK_OUT_ROOT=E:\_VlcBuildOut"
set "FINAL_ROOT=E:\Unreal Projects\5.4\VlcMedia_UE_Packaged\_Packed"
rem ================================================================================

title Package VlcMedia Plugin

echo.
echo === CLEAN OUTPUT ===
if exist "%PACK_OUT_ROOT%" rd /s /q "%PACK_OUT_ROOT%" 2>nul
if not exist "%PACK_OUT_ROOT%" md "%PACK_OUT_ROOT%"
if not exist "%FINAL_ROOT%" md "%FINAL_ROOT%"

rem Derive names/paths
for %%I in ("%PLUGIN_UPLUGIN%") do set "PLUGIN_DIR=%%~dpI"
if "%PLUGIN_DIR:~-1%"=="\" set "PLUGIN_DIR=%PLUGIN_DIR:~0,-1%"
for %%J in ("%PLUGIN_DIR%") do set "PLUGIN_NAME=%%~nxJ"
set "FINAL_DIR=%FINAL_ROOT%\%PLUGIN_NAME%"
set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

echo.
echo === BUILD PLUGIN PACKAGE ===
call "%UE_DIR%\Engine\Build\BatchFiles\RunUAT.bat" ^
  BuildPlugin -Plugin="%PLUGIN_UPLUGIN%" -Package="%PACK_OUT_ROOT%" ^
  -CreateSubFolder -nocompile -nocompileuat
if errorlevel 1 goto :fail_build

echo.
echo === LOCATE PACKAGED PLUGIN ROOT ===
set "PKG_UPLUGIN="
for /f "delims=" %%F in ('dir /b /s "%PACK_OUT_ROOT%\%PLUGIN_NAME%.uplugin" 2^>nul') do if not defined PKG_UPLUGIN set "PKG_UPLUGIN=%%F"
if not defined PKG_UPLUGIN for /f "delims=" %%F in ('dir /b /s "%PACK_OUT_ROOT%\*.uplugin" 2^>nul') do if not defined PKG_UPLUGIN set "PKG_UPLUGIN=%%F"
if not defined PKG_UPLUGIN goto :fail_find_uplugin

for %%I in ("%PKG_UPLUGIN%") do set "PKG_ROOT=%%~dpI"
if "%PKG_ROOT:~-1%"=="\" set "PKG_ROOT=%PKG_ROOT:~0,-1%"

echo Found .uplugin at:
echo   %PKG_UPLUGIN%
echo Packaged root:
echo   %PKG_ROOT%

echo.
echo === FLATTEN TO FINAL DIR ===
if exist "%FINAL_DIR%" rd /s /q "%FINAL_DIR%" 2>nul
md "%FINAL_DIR%" 1>nul 2>nul
robocopy "%PKG_ROOT%" "%FINAL_DIR%" /E /R:1 /W:1 /NFL /NDL /NJH /NJS /NP >nul
set "RC=%ERRORLEVEL%"
echo robocopy rc=%RC% 0..7 means success

echo.
echo === ENSURE ThirdParty\vlc IN FINAL OUTPUT ===
echo Probe 1: %PLUGIN_DIR%\ThirdParty\vlc
echo Probe 2: %PKG_ROOT%\ThirdParty\vlc
echo Probe 3: %SCRIPT_DIR%\ThirdParty\vlc

set "VLC_SRC="
call :TryVlc "%PLUGIN_DIR%\ThirdParty\vlc" source_plugin
if not defined VLC_SRC call :TryVlc "%PKG_ROOT%\ThirdParty\vlc" packaged_root
if not defined VLC_SRC call :TryVlc "%SCRIPT_DIR%\ThirdParty\vlc" bat_folder
if not defined VLC_SRC goto :fail_no_vlc

echo Using VLC source:
echo   %VLC_SRC%
echo Copying VLC runtime into:
echo   %FINAL_DIR%\ThirdParty\vlc
robocopy "%VLC_SRC%" "%FINAL_DIR%\ThirdParty\vlc" /E /R:1 /W:1 /NFL /NDL /NJH /NJS /NP >nul

echo.
echo === VERIFY VLC RUNTIME [flat folder] ===
if exist "%FINAL_DIR%\ThirdParty\vlc\Win64\libvlc.dll"       echo OK libvlc.dll       || goto :fail_verify_flat
if exist "%FINAL_DIR%\ThirdParty\vlc\Win64\libvlccore.dll"   echo OK libvlccore.dll   || goto :fail_verify_flat
if exist "%FINAL_DIR%\ThirdParty\vlc\Win64\plugins"          echo OK plugins dir      || goto :fail_verify_flat

echo.
echo === CREATE READY-TO-DROP Plugins WRAPPER ===
set "WRAP_DIR=%FINAL_ROOT%\Plugins\%PLUGIN_NAME%"
if exist "%WRAP_DIR%" rd /s /q "%WRAP_DIR%" 2>nul
md "%WRAP_DIR%" 1>nul 2>nul
robocopy "%FINAL_DIR%" "%WRAP_DIR%" /E /R:1 /W:1 /NFL /NDL /NJH /NJS /NP >nul

echo.
echo === VERIFY VLC RUNTIME [Plugins wrapper] ===
if exist "%WRAP_DIR%\ThirdParty\vlc\Win64\libvlc.dll"       echo OK wrapper libvlc.dll       || goto :fail_verify_wrap
if exist "%WRAP_DIR%\ThirdParty\vlc\Win64\libvlccore.dll"   echo OK wrapper libvlccore.dll   || goto :fail_verify_wrap
if exist "%WRAP_DIR%\ThirdParty\vlc\Win64\plugins"          echo OK wrapper plugins dir      || goto :fail_verify_wrap

rem Write README without using a block (no parentheses)
> "%FINAL_ROOT%\Plugins\README.txt"  echo To install:
>>"%FINAL_ROOT%\Plugins\README.txt" echo 1. Copy the Plugins folder to the root of your Unreal project (next to YourProject.uproject).
>>"%FINAL_ROOT%\Plugins\README.txt" echo 2. Ensure Plugins\%PLUGIN_NAME%\ThirdParty\vlc\Win64 remains intact (libvlc.dll, libvlccore.dll, plugins\...).
>>"%FINAL_ROOT%\Plugins\README.txt" echo 3. Launch Unreal and enable the plugin if needed.

echo.
echo === DONE ===
echo Flat plugin folder:
echo   %FINAL_DIR%
echo Ready-to-drop wrapper:
echo   %FINAL_ROOT%\Plugins
exit /b 0


rem ================================================================================
rem                                   HELPERS
rem ================================================================================
:TryVlc
rem %~1 = candidate ThirdParty\vlc, %2 = label
if exist "%~1\Win64\libvlc.dll" set "VLC_SRC=%~1" & echo   Found VLC at: %~1   label=%~2
exit /b 0

rem ================================================================================
rem                                   FAIL PATHS
rem ================================================================================
:fail_build
echo BUILD FAILED
exit /b 1

:fail_find_uplugin
echo ERROR: Could not find packaged .uplugin under %PACK_OUT_ROOT%
exit /b 1

:fail_no_vlc
echo ERROR: Could not find a valid ThirdParty\vlc\Win64 with libvlc.dll.
echo Checked:
echo   %PLUGIN_DIR%\ThirdParty\vlc
echo   %PKG_ROOT%\ThirdParty\vlc
echo   %SCRIPT_DIR%\ThirdParty\vlc
exit /b 1

:fail_verify_flat
echo ERROR: Missing VLC runtime in flat output under %FINAL_DIR%\ThirdParty\vlc\Win64
exit /b 1

:fail_verify_wrap
echo ERROR: Missing VLC runtime in Plugins wrapper under %WRAP_DIR%\ThirdParty\vlc\Win64
exit /b 1
