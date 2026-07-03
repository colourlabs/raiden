@echo off
setlocal enabledelayedexpansion

set TOOLS_DIR=%~dp0..\tools
set BIN_DIR=%TOOLS_DIR%\bin
set VERSION_FILE=%TOOLS_DIR%\.versions

if not exist "%TOOLS_DIR%" mkdir "%TOOLS_DIR%"
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"

echo raiden engine - Windows Setup
echo ========================================
echo [1/5] Checking prerequisites...
echo.

where cmake >nul 2>&1
if %errorlevel% neq 0 (
  echo   [FAIL] CMake not found. Install from: https://cmake.org/download/
  echo.
) else (
  echo   [OK]   CMake found
)

where git >nul 2>&1
if %errorlevel% neq 0 (
  echo   [FAIL] Git not found. Install from: https://git-scm.com/download/win
  echo.
) else (
  echo   [OK]   Git found
)

where cl >nul 2>&1
if %errorlevel% neq 0 (
  echo   [FAIL] MSVC compiler not found. Install Visual Studio 2026 or 2022 with the
  echo          "Desktop development with C++" workload.
  echo.
) else (
  echo   [OK]   MSVC compiler found
)

if "%VULKAN_SDK%"=="" (
  echo   [FAIL] Vulkan SDK not found. Install from:
  echo          https://vulkan.lunarg.com/sdk/home
  echo          Ensure VULKAN_SDK environment variable is set.
  echo.
) else (
  echo   [OK]   Vulkan SDK found at %VULKAN_SDK%
)

echo.
echo [2/5] Downloading Slang shader compiler...
echo.

set SLANG_URL=https://github.com/shader-slang/slang/releases/download/v2026.12.2/slang-2026.12.2-windows-x86_64.zip
set SLANG_ZIP=%TOOLS_DIR%\slang.zip

if not exist "%BIN_DIR%\slangc.exe" (
  echo   Downloading slangc from GitHub releases...
  powershell -Command "& {[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '%SLANG_URL%' -OutFile '%SLANG_ZIP%'}"
  if !errorlevel! neq 0 (
    echo   [FAIL] Download failed. Check your internet connection.
    goto :error
  )
  echo   Extracting...
  powershell -Command "& {Expand-Archive -Path '%SLANG_ZIP%' -DestinationPath '%TOOLS_DIR%\slang_tmp' -Force}"
  if exist "%TOOLS_DIR%\slang_tmp\bin\slangc.exe" (
    copy "%TOOLS_DIR%\slang_tmp\bin\slangc.exe" "%BIN_DIR%\slangc.exe" >nul
    echo   [OK]   slangc installed to %BIN_DIR%
  ) else (
    if exist "%TOOLS_DIR%\slang_tmp\slangc.exe" (
      copy "%TOOLS_DIR%\slang_tmp\slangc.exe" "%BIN_DIR%\slangc.exe" >nul
      echo   [OK]   slangc installed to %BIN_DIR%
    ) else (
      echo   [FAIL] Extraction failed.
      goto :error
    )
  )
  rmdir /s /q "%TOOLS_DIR%\slang_tmp" 2>nul
  del "%SLANG_ZIP%"
) else (
  echo   [OK]   slangc already present at %BIN_DIR%\slangc.exe
)

echo.
echo [3/5] Downloading KTX tools (toktx)...
echo.

set KTX_URL=https://github.com/KhronosGroup/KTX-Software/releases/download/v4.4.2/KTX-Software-4.4.2-Windows-x64.exe
set KTX_INSTALLER=%TOOLS_DIR%\ktx_installer.exe
set KTX_EXTRACT_DIR=%TOOLS_DIR%\ktx

if not exist "%BIN_DIR%\toktx.exe" (
  echo   Downloading KTX-Software installer from GitHub releases...
  powershell -Command "& {[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '%KTX_URL%' -OutFile '%KTX_INSTALLER%'}"
  if !errorlevel! neq 0 (
    echo   [FAIL] Download failed. Check your internet connection.
    goto :error
  )
  echo   Installing (silent mode)...
  if not exist "%KTX_EXTRACT_DIR%" mkdir "%KTX_EXTRACT_DIR%"
  start /wait "" "%KTX_INSTALLER%" /S /D=%KTX_EXTRACT_DIR%
  set KTX_INSTALL_OK=!errorlevel!
  if !KTX_INSTALL_OK! equ 0 (
    if exist "%KTX_EXTRACT_DIR%\toktx.exe" (
      copy "%KTX_EXTRACT_DIR%\toktx.exe" "%BIN_DIR%\toktx.exe" >nul
    ) else if exist "%KTX_EXTRACT_DIR%\bin\toktx.exe" (
      copy "%KTX_EXTRACT_DIR%\bin\toktx.exe" "%BIN_DIR%\toktx.exe" >nul
    ) else (
      dir /s /b "%KTX_EXTRACT_DIR%\toktx.exe" > "%TEMP%\ktx_path.txt" 2>nul
      set /p KTX_PATH=<"%TEMP%\ktx_path.txt"
      if defined KTX_PATH (
        copy "!KTX_PATH!" "%BIN_DIR%\toktx.exe" >nul
      )
    )
  ) else (
    echo   [WARN] Silent install failed, searching PATH for existing toktx...
  )
  del "%KTX_INSTALLER%"
  where toktx >nul 2>&1
  if !errorlevel! equ 0 (
    echo   [OK]   toktx found on PATH
  ) else if exist "%BIN_DIR%\toktx.exe" (
    echo   [OK]   toktx installed to %BIN_DIR%
  ) else (
    echo   [WARN] toktx not found. You may need to install KTX-Software
    echo          manually from: https://github.com/KhronosGroup/KTX-Software
    echo          Then ensure toktx is on your PATH.
  )
) else (
  echo   [OK]   toktx already present at %BIN_DIR%\toktx.exe
)

echo.
echo [4/5] Updating environment...
echo.

set "PATH=%BIN_DIR%;%PATH%"

echo   Tools directory: %BIN_DIR%
echo   This directory has been added to PATH for this session.
echo   To make permanent, add %BIN_DIR% to your user/system PATH.
echo.
echo   Alternatively, run: setx PATH "%%PATH%%;%BIN_DIR%"

echo.
echo [5/5] Summary
echo.

echo   slangc.exe  :
if exist "%BIN_DIR%\slangc.exe" (echo       [OK]   %BIN_DIR%\slangc.exe) else (echo       [MISS] Not found)
echo.
echo   toktx.exe   :
if exist "%BIN_DIR%\toktx.exe" (echo       [OK]   %BIN_DIR%\toktx.exe) else (echo       [MISS] Not found)
echo.
echo   If any tools are missing, check the warnings above.
echo.
echo  Setup complete.
goto :eof

:error
echo.
echo [FAIL] Setup encountered an error. Check the messages above.
exit /b 1
