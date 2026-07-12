"""raiden editor [--preset NAME] [--datapack PATH]"""

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
        help="Path to the datapack directory (optional, shows launcher if omitted)",
    )
    sub.add_argument(
        "--no-datapack",
        action="store_true",
        help="Launch editor standalone without a datapack (skips project launcher)",
    )

def _find_editor_binary(build_dir: Path) -> Path | None:
    """Search for EditorRuntime under build_dir."""
    exe_name = "EditorRuntime.exe" if sys.platform == "win32" else "EditorRuntime"
    for candidate in build_dir.rglob(exe_name):
        if candidate.is_file():
            if sys.platform != "win32" and not os.access(candidate, os.X_OK):
                continue
            return candidate
    return None

def run_editor(args, project_root: Path) -> int:
    if args.preset:
        build_dir = project_root / "build" / args.preset
    else:
        build_dir = detect_build_dir(args.mode, project_root)

    binary = _find_editor_binary(build_dir)
    if binary is None:
        print("[ERROR] EditorRuntime not found under build/.", file=sys.stderr)
        print("        Build the project first with: raiden build", file=sys.stderr)
        return 1

    print(f"  [OK]   Binary found: {binary}")

    cmd_args = []
    if args.no_datapack:
        print("  [INFO] Launching standalone, no datapack.")
    elif args.datapack:
        cmd_args.extend(["--datapack", args.datapack])
        print(f"  [OK]   Datapack: {args.datapack}")
    else:
        print("  [INFO] No datapack specified, editor will show project launcher.")

    print(f"  Running {binary.name}...\n")
    return subprocess.run([str(binary)] + cmd_args).returncode
