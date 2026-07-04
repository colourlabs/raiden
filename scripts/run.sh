#!/usr/bin/env bash
set -euo pipefail
shopt -s nullglob

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/.."

cd "$PROJECT_DIR"
BINARY=""
for dir in \
  build/linux-debug/src/Runtime/EngineRuntime \
  build/default/src/Runtime/EngineRuntime \
  build/*/src/Runtime/EngineRuntime \
  build/src/Runtime/EngineRuntime; do
  for f in $dir; do
    if [ -x "$f" ]; then
      BINARY="$f"
      break 2
    fi
  done
done

if [ -z "$BINARY" ]; then
  echo "[ERROR] EngineRuntime binary not found under build/."
  echo "        Build the project first with: ./scripts/build.sh"
  exit 1
fi

echo "  [OK]   Binary found: $BINARY"

# auto-detect datapack when --datapack is not given
ARGS=("$@")
HAS_DATAPACK=false
for arg in "$@"; do
  if [ "$arg" = "--datapack" ] || [[ "$arg" == --datapack=* ]]; then
    HAS_DATAPACK=true
    break
  fi
done

if [ "$HAS_DATAPACK" = false ]; then
  BUILD_PREFIX="${BINARY%/src/Runtime/EngineRuntime}"
  DATAPACK=$(find "$BUILD_PREFIX" -name 'engine.toml' -path '*/ExampleGame/*' -exec dirname {} \; 2>/dev/null | head -1)
  if [ -z "$DATAPACK" ]; then
    DATAPACK=$(find "$PROJECT_DIR/build" -name 'engine.toml' -path '*/ExampleGame/*' -exec dirname {} \; 2>/dev/null | head -1)
  fi
  if [ -n "$DATAPACK" ]; then
    echo "  [OK]   Datapack auto-detected: $DATAPACK"
    ARGS+=(--datapack "$DATAPACK")
  else
    echo "  [WARN] No game datapack found under build/."
  fi
fi

echo "  Running natively (no Steam Linux Runtime)..."
echo "  (once running, find it with: pgrep -f EngineRuntime)"
echo

exec "$BINARY" "${ARGS[@]}"