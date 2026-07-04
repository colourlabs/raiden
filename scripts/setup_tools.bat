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

call :CheckMSVC

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

call :InstallSlang
if errorlevel 1 goto :error

echo.
echo [3/5] Downloading KTX tools (toktx)...
echo.

set KTX_URL=https://github.com/KhronosGroup/KTX-Software/releases/download/v4.4.2/KTX-Software-4.4.2-Windows-x64.exe
set KTX_INSTALLER=%TOOLS_DIR%\ktx_installer.exe
set KTX_EXTRACT_DIR=%TOOLS_DIR%\ktx

call :InstallKtx
if errorlevel 1 goto :error

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

:CheckMSVC
set MSVC_FOUND=0
set "VS_INSTALL_PATH="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

where cl >nul 2>&1
if %errorlevel% equ 0 set MSVC_FOUND=1

if %MSVC_FOUND% equ 1 goto :CheckMSVC_Report
if not exist "%VSWHERE%" goto :CheckMSVC_Report

for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
  set "VS_INSTALL_PATH=%%I"
  set MSVC_FOUND=1
)

:CheckMSVC_Report
if %MSVC_FOUND% neq 1 goto :CheckMSVC_Fail
if defined VS_INSTALL_PATH goto :CheckMSVC_FoundViaVSWhere
echo   [OK]   MSVC compiler found
exit /b 0

:CheckMSVC_FoundViaVSWhere
echo   [OK]   MSVC compiler found via Visual Studio at %VS_INSTALL_PATH%
echo          (run vcvarsall.bat / open a Developer Command Prompt to use cl.exe)
exit /b 0

:CheckMSVC_Fail
echo   [FAIL] MSVC compiler not found. Install Visual Studio 2026 or 2022 with the
echo          "Desktop development with C++" workload.
echo.
exit /b 0

:InstallSlang
if exist "%BIN_DIR%\slangc.exe" (
  echo   [OK]   slangc already present at %BIN_DIR%\slangc.exe
  exit /b 0
)

echo   Downloading slangc from GitHub releases...
powershell -Command "& {[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '%SLANG_URL%' -OutFile '%SLANG_ZIP%'}"
if errorlevel 1 (
  echo   [FAIL] Download failed. Check your internet connection.
  exit /b 1
)

echo   Extracting...
powershell -Command "& {Expand-Archive -Path '%SLANG_ZIP%' -DestinationPath '%TOOLS_DIR%\slang_tmp' -Force}"

if exist "%TOOLS_DIR%\slang_tmp\bin" (
  xcopy "%TOOLS_DIR%\slang_tmp\bin\*" "%BIN_DIR%\" /Y /I >nul
) else (
  xcopy "%TOOLS_DIR%\slang_tmp\*.exe" "%BIN_DIR%\" /Y /I >nul
  xcopy "%TOOLS_DIR%\slang_tmp\*.dll" "%BIN_DIR%\" /Y /I >nul
)

rmdir /s /q "%TOOLS_DIR%\slang_tmp" 2>nul
del "%SLANG_ZIP%" 2>nul

if not exist "%BIN_DIR%\slangc.exe" (
  echo   [FAIL] Extraction failed.
  exit /b 1
)
echo   [OK]   slangc installed to %BIN_DIR%
exit /b 0

:InstallKtx
if exist "%BIN_DIR%\toktx.exe" (
  echo   [OK]   toktx already present at %BIN_DIR%\toktx.exe
  exit /b 0
)

echo   Downloading KTX-Software installer from GitHub releases...
powershell -Command "& {[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '%KTX_URL%' -OutFile '%KTX_INSTALLER%'}"
if errorlevel 1 (
  echo   [FAIL] Download failed. Check your internet connection.
  exit /b 1
)

echo   Installing (silent mode)...
if not exist "%KTX_EXTRACT_DIR%" mkdir "%KTX_EXTRACT_DIR%"

start /wait "" "%KTX_INSTALLER%" /S /D=%KTX_EXTRACT_DIR%
if errorlevel 1 echo   [WARN] Silent install failed

if exist "%KTX_EXTRACT_DIR%\bin" (
  xcopy "%KTX_EXTRACT_DIR%\bin\*" "%BIN_DIR%\" /Y /I >nul
) else (
  xcopy "%KTX_EXTRACT_DIR%\*.exe" "%BIN_DIR%\" /Y /I >nul
  xcopy "%KTX_EXTRACT_DIR%\*.dll" "%BIN_DIR%\" /Y /I >nul
)

if not exist "%BIN_DIR%\toktx.exe" (
  for /r "%KTX_EXTRACT_DIR%" %%F in (toktx.exe) do (
    if not exist "%BIN_DIR%\toktx.exe" (
      for %%D in ("%%F") do (
        xcopy "%%~dpD*.exe" "%BIN_DIR%\" /Y /I >nul
        xcopy "%%~dpD*.dll" "%BIN_DIR%\" /Y /I >nul
      )
    )
  )
)

del "%KTX_INSTALLER%" 2>nul

where toktx >nul 2>&1
if %errorlevel% equ 0 (
  echo   [OK]   toktx found on PATH
  exit /b 0
)
if exist "%BIN_DIR%\toktx.exe" (
  echo   [OK]   toktx installed to %BIN_DIR%
  exit /b 0
)
echo   [WARN] toktx not found. You may need to install KTX-Software manually.
exit /b 0
