@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..
cd /d "%PROJECT_DIR%"

:: locate the EngineRuntime binary
set BINARY=
for %%d in (
  "build\windows-ninja\src\Runtime\EngineRuntime.exe"
  "build\windows-vs2022\src\Runtime\EngineRuntime.exe"
  "build\windows-vs2026\src\Runtime\EngineRuntime.exe"
  "build\default\src\Runtime\EngineRuntime.exe"
  "build\src\Runtime\EngineRuntime.exe"
) do (
  if exist "%%~d" set "BINARY=%%~d" & goto :found
)

for /d %%p in (build\*) do (
  if exist "%%p\src\Runtime\Debug\EngineRuntime.exe" set "BINARY=%%p\src\Runtime\Debug\EngineRuntime.exe" & goto :found
  if exist "%%p\src\Runtime\Release\EngineRuntime.exe" set "BINARY=%%p\src\Runtime\Release\EngineRuntime.exe" & goto :found
  if exist "%%p\src\Runtime\RelWithDebInfo\EngineRuntime.exe" set "BINARY=%%p\src\Runtime\RelWithDebInfo\EngineRuntime.exe" & goto :found
)

:: fallback: any flat build with wildcard
for /d %%p in (build\*) do (
  if exist "%%p\src\Runtime\EngineRuntime.exe" set "BINARY=%%p\src\Runtime\EngineRuntime.exe" & goto :found
)

:found
if "%BINARY%"=="" (
  echo [ERROR] EngineRuntime.exe not found under build/.
  echo        Build the project first with: scripts\build
  exit /b 1
)

echo   [OK]   Binary found: %BINARY%

:: auto-detect datapack when --datapack not given
set HAS_DATAPACK=0
set ARGS=%*

echo %ARGS% | findstr /C:"--datapack" >nul
if !errorlevel! neq 0 (
  :: look for engine.toml in the same build prefix
  set BUILD_PREFIX=%BINARY:\src\Runtime\EngineRuntime.exe=%
  if exist "!BUILD_PREFIX!\games\ExampleGame\engine.toml" (
    set DATAPACK=!BUILD_PREFIX!\games\ExampleGame
    echo   [OK]   Datapack auto-detected: !DATAPACK!
    set ARGS=%ARGS% --datapack "!DATAPACK!"
  ) else (
    :: scan for any ExampleGame\engine.toml under build\
    for /r build %%f in (engine.toml) do (
      echo %%f | findstr /C:"ExampleGame" >nul
      if !errorlevel! equ 0 (
        set "DATAPACK=%%~dpf"
        set "DATAPACK=!DATAPACK:~0,-1!"
        echo   [OK]   Datapack auto-detected: !DATAPACK!
        set ARGS=%ARGS% --datapack "!DATAPACK!"
        goto :run
      )
    )
    echo   [WARN] No game datapack found under build/.
  )
)

:run
echo   Running...
echo.
%BINARY% %ARGS%
