@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..
cd /d "%PROJECT_DIR%"

:: parse first argument: "editor" to select editor binary and datapack
set DATAPACK_MODE=game
set ARGS=
:parse_args
if "%~1"=="" goto :done_parse
if "%~1"=="editor" (
  if "!DATAPACK_MODE!"=="game" (
    set DATAPACK_MODE=editor
    shift
    goto :parse_args
  )
)
if "%~1"=="--datapack" (
  set ARGS=!ARGS! %1 %2
  shift
  shift
  goto :parse_args
)
set ARGS=!ARGS! %1
shift
goto :parse_args
:done_parse

:: locate the binary (EditorRuntime or EngineRuntime depending on mode)
set BINARY=

if "!DATAPACK_MODE!"=="editor" (
  for %%d in (
    "build\windows-ninja\src\RaidenEditor\EditorRuntime.exe"
    "build\windows-vs2022\src\RaidenEditor\EditorRuntime.exe"
    "build\windows-vs2026\src\RaidenEditor\EditorRuntime.exe"
    "build\default\src\RaidenEditor\EditorRuntime.exe"
    "build\src\RaidenEditor\EditorRuntime.exe"
  ) do (
    if exist "%%~d" set "BINARY=%%~d" & goto :found
  )

  for /d %%p in (build\*) do (
    if exist "%%p\src\RaidenEditor\Debug\EditorRuntime.exe" set "BINARY=%%p\src\RaidenEditor\Debug\EditorRuntime.exe" & goto :found
    if exist "%%p\src\RaidenEditor\Release\EditorRuntime.exe" set "BINARY=%%p\src\RaidenEditor\Release\EditorRuntime.exe" & goto :found
    if exist "%%p\src\RaidenEditor\RelWithDebInfo\EditorRuntime.exe" set "BINARY=%%p\src\RaidenEditor\RelWithDebInfo\EditorRuntime.exe" & goto :found
  )

  :: fallback: any flat build with wildcard
  for /d %%p in (build\*) do (
    if exist "%%p\src\RaidenEditor\EditorRuntime.exe" set "BINARY=%%p\src\RaidenEditor\EditorRuntime.exe" & goto :found
  )
) else (
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
)

:found
if "%BINARY%"=="" (
  echo [ERROR] Binary not found under build/.
  echo        Build the project first with: scripts\build
  exit /b 1
)

echo   [OK]   Binary found: %BINARY%

:: auto-detect datapack when --datapack not given
echo !ARGS! | findstr /C:"--datapack" >nul
if !errorlevel! neq 0 (
  set BUILD_PREFIX=%BINARY:\src\RaidenEditor\EditorRuntime.exe=%
  set BUILD_PREFIX=!BUILD_PREFIX:\src\Runtime\EngineRuntime.exe=!

  if "!DATAPACK_MODE!"=="editor" (
    if exist "!BUILD_PREFIX!\editor\RaidenEditor\engine.toml" (
      set DATAPACK=!BUILD_PREFIX!\editor\RaidenEditor
      echo   [OK]   Editor datapack auto-detected: !DATAPACK!
      set ARGS=!ARGS! --datapack "!DATAPACK!"
    ) else (
      for /r build %%f in (engine.toml) do (
        echo %%f | findstr /C:"RaidenEditor" >nul
        if !errorlevel! equ 0 (
          set "DATAPACK=%%~dpf"
          set "DATAPACK=!DATAPACK:~0,-1!"
          echo   [OK]   Editor datapack auto-detected: !DATAPACK!
          set ARGS=!ARGS! --datapack "!DATAPACK!"
          goto :run
        )
      )
      echo   [WARN] No editor datapack found under build/.
    )
  ) else (
    if exist "!BUILD_PREFIX!\games\ExampleGame\engine.toml" (
      set DATAPACK=!BUILD_PREFIX!\games\ExampleGame
      echo   [OK]   Datapack auto-detected: !DATAPACK!
      set ARGS=!ARGS! --datapack "!DATAPACK!"
    ) else (
      for /r build %%f in (engine.toml) do (
        echo %%f | findstr /C:"ExampleGame" >nul
        if !errorlevel! equ 0 (
          set "DATAPACK=%%~dpf"
          set "DATAPACK=!DATAPACK:~0,-1!"
          echo   [OK]   Datapack auto-detected: !DATAPACK!
          set ARGS=!ARGS! --datapack "!DATAPACK!"
          goto :run
        )
      )
      echo   [WARN] No game datapack found under build/.
    )
  )
)

:run
echo   Running...
echo.
%BINARY% %ARGS%
