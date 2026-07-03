#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TOOLS_DIR="$SCRIPT_DIR/../tools"
BIN_DIR="$TOOLS_DIR/bin"

mkdir -p "$TOOLS_DIR" "$BIN_DIR"

echo "raiden engine - Linux Setup"
echo "========================================"
echo

echo "[1/5] Checking prerequisites..."
echo

check_cmd() {
  if command -v "$1" &>/dev/null; then
    echo "  [OK]   $1 found"
    return 0
  else
    echo "  [FAIL] $1 not found - $2"
    return 1
  fi
}

check_cmd cmake "Install from: https://cmake.org/download/"
check_cmd git "Install from: https://git-scm.com/download/linux"
check_cmd g++ "Install g++ (or clang++) from your package manager"
check_cmd make "Install make from your package manager"

if [ -z "${VULKAN_SDK:-}" ]; then
  echo "  [FAIL] Vulkan SDK not found."
  echo "         Install from: https://vulkan.lunarg.com/sdk/home"
  echo "         Or install vulkan-sdk / vulkan-headers from your package manager."
  echo "         Ensure VULKAN_SDK is set."
else
  echo "  [OK]   Vulkan SDK found at $VULKAN_SDK"
fi

echo
echo "[2/5] Downloading Slang shader compiler..."
echo

SLANG_URL="https://github.com/shader-slang/slang/releases/download/v2026.12.2/slang-2026.12.2-linux-x86_64.tar.gz"

if [ ! -f "$BIN_DIR/slangc" ]; then
  echo "  Downloading slangc from GitHub releases..."
  curl -fsSL "$SLANG_URL" -o "$TOOLS_DIR/slang.tar.gz"
  echo "  Extracting..."
  tar -xzf "$TOOLS_DIR/slang.tar.gz" -C "$TOOLS_DIR"
  if [ -d "$TOOLS_DIR/slang-2026.12.2" ]; then
    mv "$TOOLS_DIR/slang-2026.12.2" "$TOOLS_DIR/slang"
  fi
  if [ -f "$TOOLS_DIR/slang/bin/slangc" ]; then
    cp "$TOOLS_DIR/slang/bin/slangc" "$BIN_DIR/slangc"
    chmod +x "$BIN_DIR/slangc"
    echo "  [OK]   slangc installed to $BIN_DIR"
  else
    echo "  [FAIL] Could not find slangc binary after extraction."
    exit 1
  fi
  rm -f "$TOOLS_DIR/slang.tar.gz"
else
  echo "  [OK]   slangc already present at $BIN_DIR/slangc"
fi

echo
echo "[3/5] Downloading KTX tools (toktx)..."
echo

KTX_URL="https://github.com/KhronosGroup/KTX-Software/releases/download/v4.4.2/KTX-Software-4.4.2-Linux-x86_64.tar.bz2"

if [ ! -f "$BIN_DIR/toktx" ]; then
  echo "  Downloading KTX-Software from GitHub releases..."
  curl -fsSL "$KTX_URL" -o "$TOOLS_DIR/ktx.tar.bz2"
  echo "  Extracting..."
  mkdir -p "$TOOLS_DIR/ktx"
  tar -xjf "$TOOLS_DIR/ktx.tar.bz2" -C "$TOOLS_DIR/ktx"
  TOKTX_BIN=$(find "$TOOLS_DIR/ktx" -name 'toktx' -type f 2>/dev/null | head -1)
  if [ -n "$TOKTX_BIN" ]; then
    cp "$TOKTX_BIN" "$BIN_DIR/toktx"
    chmod +x "$BIN_DIR/toktx"
    echo "  [OK]   toktx installed to $BIN_DIR"
  else
    echo "  [FAIL] Could not find toktx binary after extraction."
    exit 1
  fi
  rm -f "$TOOLS_DIR/ktx.tar.bz2"
else
  echo "  [OK]   toktx already present at $BIN_DIR/toktx"
fi

echo
echo "[4/5] Updating environment..."
echo

export PATH="$BIN_DIR:$PATH"
echo "  Tools directory: $BIN_DIR"
echo "  Added to PATH for this session."
echo "  To make permanent, add to your ~/.bashrc or ~/.zshrc:"
echo "    export PATH=\"\$PATH:$BIN_DIR\""

echo
echo "[5/5] Summary"
echo

echo "  slangc  : $( [ -f "$BIN_DIR/slangc" ] && echo '[OK]' || echo '[MISS]' )"
echo "  toktx   : $( [ -f "$BIN_DIR/toktx" ] && echo '[OK]' || echo '[MISS]' )"
echo
echo "  Setup complete."
