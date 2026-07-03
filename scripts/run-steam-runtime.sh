#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/.."

# locate the Steam Linux Runtime
STEAM_RUNTIME=""
for candidate in \
  "$HOME/.steam/root/steamapps/common/SteamLinuxRuntime_sniper/run" \
  "$HOME/.steam/steam/steamapps/common/SteamLinuxRuntime_sniper/run" \
  "$HOME/.local/share/Steam/steamapps/common/SteamLinuxRuntime_sniper/run" \
  "$HOME/.steam/root/steamapps/common/SteamLinuxRuntime_soldier/run" \
  "$HOME/.steam/steam/steamapps/common/SteamLinuxRuntime_soldier/run" \
  "$HOME/.local/share/Steam/steamapps/common/SteamLinuxRuntime_soldier/run"; do
  if [ -x "$candidate" ]; then
    STEAM_RUNTIME="$candidate"
    break
  fi
done

if [ -z "$STEAM_RUNTIME" ]; then
  echo "[ERROR] Steam Linux Runtime not found."
  echo "        Install Steam, then subscribe to 'Steam Linux Runtime' in your library."
  echo
  echo "        Searched:"
  for candidate in \
    "\$HOME/.steam/root/steamapps/common/SteamLinuxRuntime_sniper/run" \
    "\$HOME/.steam/steam/steamapps/common/SteamLinuxRuntime_sniper/run" \
    "\$HOME/.local/share/Steam/steamapps/common/SteamLinuxRuntime_sniper/run"; do
    echo "          $candidate"
  done
  exit 1
fi

echo "  [OK]   Steam Linux Runtime found: $STEAM_RUNTIME"

cd "$PROJECT_DIR"
BINARY=""
for dir in \
  build/linux-debug/src/Runtime/EngineRuntime \
  build/default/src/Runtime/EngineRuntime \
  build/*/src/Runtime/EngineRuntime \
  build/src/Runtime/EngineRuntime; do
  # expand globs to first match
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
  if [ "$arg" = "--datapack" ]; then
    HAS_DATAPACK=true
    break
  fi
done

if [ "$HAS_DATAPACK" = false ]; then
  # derive the build prefix from the binary path and look for a game datapack there
  BUILD_PREFIX=$(echo "$BINARY" | sed 's|/src/Runtime/EngineRuntime$||')
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

echo "  Running under Steam Linux Runtime..."
echo

exec "$STEAM_RUNTIME" -- "$BINARY" "${ARGS[@]}"
