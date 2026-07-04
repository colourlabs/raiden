@echo off
setlocal enabledelayedexpansion
set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..
cd /d "%PROJECT_DIR%"

set MODE=%1
if "%MODE%"=="" set MODE=debug

set CONFIG=Debug
if /I "%MODE%"=="release" set CONFIG=Release

set PRESET=
set BUILD_PRESET=
set NEEDS_CONFIG=0

where ninja >nul 2>&1
if %errorlevel% equ 0 (
  set PRESET=windows-ninja
  set BUILD_PRESET=windows-ninja
  if /I "%MODE%"=="release" set BUILD_PRESET=windows-ninja-release
  goto :build
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_FULL_VERSION="
set "VS_MAJOR="

if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%V in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion`) do set "VS_FULL_VERSION=%%V"
)

if defined VS_FULL_VERSION for /f "tokens=1 delims=." %%M in ("%VS_FULL_VERSION%") do set "VS_MAJOR=%%M"

if "%VS_MAJOR%"=="18" (
  set PRESET=windows-vs2026
  set BUILD_PRESET=windows-vs2026-debug
  if /I "%MODE%"=="release" set BUILD_PRESET=windows-vs2026-release
  set NEEDS_CONFIG=1
  goto :build
)

if "%VS_MAJOR%"=="17" (
  set PRESET=windows-vs2022
  set BUILD_PRESET=windows-vs2022-debug
  if /I "%MODE%"=="release" set BUILD_PRESET=windows-vs2022-release
  set NEEDS_CONFIG=1
  goto :build
)

echo [ERROR] No suitable generator found. Install Ninja or Visual Studio ^(with the "Desktop development with C++" workload^).
exit /b 1

:build
if exist "%SCRIPT_DIR%setup_tools.bat" (
  call "%SCRIPT_DIR%setup_tools.bat"
  if !errorlevel! neq 0 echo [WARNING] setup_tools.bat exited with code !errorlevel!
)

set "BASH_EXE="
for /f "usebackq tokens=*" %%B in (`where bash 2^>nul`) do if not defined BASH_EXE set "BASH_EXE=%%B"

if not defined BASH_EXE (
  set "GIT_EXE="
  for /f "usebackq tokens=*" %%G in (`where git 2^>nul`) do if not defined GIT_EXE set "GIT_EXE=%%G"
  if defined GIT_EXE (
    for %%P in ("!GIT_EXE!") do set "GIT_BIN_DIR=%%~dpP"
    if exist "!GIT_BIN_DIR!bash.exe" set "BASH_EXE=!GIT_BIN_DIR!bash.exe"
    if not defined BASH_EXE if exist "!GIT_BIN_DIR!..\bin\bash.exe" set "BASH_EXE=!GIT_BIN_DIR!..\bin\bash.exe"
    if not defined BASH_EXE if exist "!GIT_BIN_DIR!..\usr\bin\bash.exe" set "BASH_EXE=!GIT_BIN_DIR!..\usr\bin\bash.exe"
    if not defined BASH_EXE if exist "!GIT_BIN_DIR!usr\bin\bash.exe" set "BASH_EXE=!GIT_BIN_DIR!usr\bin\bash.exe"
  )
)

if not defined BASH_EXE (
  set "SCOOP_ROOT=%USERPROFILE%\scoop"
  if defined SCOOP set "SCOOP_ROOT=%SCOOP%"
  if exist "!SCOOP_ROOT!\apps\git\current\usr\bin\bash.exe" (
    set "BASH_EXE=!SCOOP_ROOT!\apps\git\current\usr\bin\bash.exe"
  )
)

set "CMAKE_EXTRA_ARGS="
if defined BASH_EXE (
  echo   Using bash: !BASH_EXE!
  set "CMAKE_EXTRA_ARGS=-DBASH_EXECUTABLE=!BASH_EXE!"

  for %%A in ("!BASH_EXE!") do set "BASH_DIR=%%~dpA"
  set "PATH=!BASH_DIR!;%PATH%"
) else (
  echo [WARNING] Could not locate bash.exe automatically. Set BASH_EXECUTABLE manually if the build fails.
)

echo   Configuring with preset: %PRESET%
cmake --preset "%PRESET%" %CMAKE_EXTRA_ARGS%
if %errorlevel% neq 0 exit /b %errorlevel%

echo   Building with build preset: %BUILD_PRESET%
cmake --build --preset "%BUILD_PRESET%"
