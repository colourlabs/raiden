#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/.."

cd "$PROJECT_DIR"

MODE="${1:-debug}"

# auto-detect preset based on platform and mode
case "$(uname -s)" in
  Linux)
    if [ "$MODE" = "release" ]; then
      PRESET="linux-release"
    else
      PRESET="linux-debug"
    fi
    ;;
  Darwin)
    if [ "$MODE" = "release" ]; then
      PRESET="default-release"
    else
      PRESET="default"
    fi
    echo "  [INFO] macOS detected, using 'default' preset (Xcode or Ninja)"
    echo "         For Visual Studio generators, use: cmake --preset <name>"
    ;;
  *)
    echo "  [ERROR] Unsupported platform: $(uname -s)"
    exit 1
    ;;
esac

# run setup if available
if [ -f "$SCRIPT_DIR/setup_tools.sh" ]; then
  "$SCRIPT_DIR/setup_tools.sh"
fi

echo "  Configuring with preset: $PRESET"
cmake --preset "$PRESET"

echo "  Building with preset: $PRESET"
cmake --build --preset "$PRESET"
