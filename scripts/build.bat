@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..
cd /d "%PROJECT_DIR%"

set MODE=%1
if "%MODE%"=="" set MODE=debug

where ninja >nul 2>&1
if !errorlevel! equ 0 (
  if /I "%MODE%"=="release" (
    set PRESET=windows-ninja-release
  ) else (
    set PRESET=windows-ninja
  )
  goto :build
)

cmake --help 2>&1 | findstr "Visual Studio 17 2022" >nul
if !errorlevel! equ 0 (
  if /I "%MODE%"=="release" (
    set PRESET=windows-vs2022-release
  ) else (
    set PRESET=windows-vs2022-debug
  )
  goto :build
)

cmake --help 2>&1 | findstr "Visual Studio 18 2026" >nul
if !errorlevel! equ 0 (
  if /I "%MODE%"=="release" (
    set PRESET=windows-vs2026-release
  ) else (
    set PRESET=windows-vs2026-debug
  )
  goto :build
)

echo [ERROR] No suitable generator found. Install Ninja or Visual Studio.
exit /b 1

:build
if exist "%SCRIPT_DIR%setup_tools.bat" (
  call "%SCRIPT_DIR%setup_tools.bat"
)

echo   Configuring with preset: %PRESET%
cmake --preset "%PRESET%"
if %errorlevel% neq 0 exit /b %errorlevel%

echo   Building with preset: %PRESET%
cmake --build --preset "%PRESET%"
