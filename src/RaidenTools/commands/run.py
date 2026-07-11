"""raiden run [--preset NAME] [--datapack PATH] [-- editor]"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

from RaidenTools.preset import detect_build_dir

def add_arguments(sub):
    sub.add_argument(
        "mode",
        nargs="?",
        default="debug",
        choices=["debug", "release"],
        help="Build mode used to locate the binary (default: debug)",
    )
    sub.add_argument(
        "--preset",
        help="Override the CMake preset used to locate the build directory",
    )
    sub.add_argument(
        "--datapack",
        help="Path to the datapack directory (auto-detected if omitted)",
    )
    sub.add_argument(
        "extra_args",
        nargs="*",
        help="Additional arguments passed to the runtime binary",
    )


def _find_binary(build_dir: Path, target: str) -> Path | None:
    """Search for a built executable under build_dir."""
    exe_name = "EngineRuntime.exe" if sys.platform == "win32" else "EngineRuntime"
    if target == "editor":
        exe_name = "EditorRuntime.exe" if sys.platform == "win32" else "EditorRuntime"

    # common layout: build/<preset>/src/<Target>/<exe>
    # also handles VS layout: build/<preset>/src/<Target>/<Config>/<exe>
    for candidate in build_dir.rglob(exe_name):
        if candidate.is_file():
            # on unix, check executable bit
            if sys.platform != "win32" and not os.access(candidate, os.X_OK):
                continue
            return candidate
    return None


def _find_datapack(build_dir: Path, target: str) -> Path | None:
    """Search for an engine.toml under build_dir matching the target."""
    marker = "RaidenEditor" if target == "editor" else "ExampleGame"
    for toml in build_dir.rglob("engine.toml"):
        if marker in str(toml):
            return toml.parent
    return None


def run_game(args, project_root: Path) -> int:
    # determine target (editor vs game)
    target = "game"
    passthrough = []
    for arg in args.extra_args:
        if arg == "editor" and target == "game":
            target = "editor"
        else:
            passthrough.append(arg)

    # find the binary
    if args.preset:
        build_dir = project_root / "build" / args.preset
    else:
        build_dir = detect_build_dir(args.mode, project_root)

    binary = _find_binary(build_dir, target)
    if binary is None:
        print(f"[ERROR] {('EditorRuntime' if target == 'editor' else 'EngineRuntime')} "
              f"not found under build/.", file=sys.stderr)
        print("        Build the project first with: raiden build", file=sys.stderr)
        return 1

    print(f"  [OK]   Binary found: {binary}")

    # resolve extra args
    cmd_args = list(passthrough)

    # auto-detect datapack if --datapack not given
    has_datapack = any(a == "--datapack" or a.startswith("--datapack=") for a in cmd_args)
    if not has_datapack and args.datapack is None:
        datapack = _find_datapack(build_dir, target)
        if datapack:
            label = "Editor datapack" if target == "editor" else "Datapack"
            print(f"  [OK]   {label} auto-detected: {datapack}")
            cmd_args.extend(["--datapack", str(datapack)])
        elif target == "editor":
            print("  [INFO] No datapack found, editor will show project launcher.")
        else:
            print("  [WARN] No datapack found under build/.")
    elif args.datapack:
        cmd_args.extend(["--datapack", args.datapack])

    print(f"  Running {binary.name}...\n")
    return subprocess.run([str(binary)] + cmd_args).returncode
