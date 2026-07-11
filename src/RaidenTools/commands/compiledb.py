"""raiden compiledb - regenerate compile_commands.json via CMake configure."""

from __future__ import annotations

import shutil
import subprocess
from pathlib import Path

from RaidenTools.preset import detect_preset


def add_arguments(sub):
    sub.add_argument(
        "mode",
        nargs="?",
        default="debug",
        choices=["debug", "release"],
        help="Build mode (default: debug)",
    )
    sub.add_argument(
        "--preset",
        help="Override the auto-detected CMake preset",
    )


def run_compiledb(args, project_root: Path) -> int:
    if args.preset:
        configure_preset = args.preset
    else:
        configure_preset, _ = detect_preset(args.mode, project_root)

    print(f"  Configuring with preset: {configure_preset}")
    r = subprocess.run(
        ["cmake", "--preset", configure_preset],
        cwd=project_root,
    )
    if r.returncode != 0:
        return r.returncode

    # Copy compile_commands.json to project root for editor/IDE convenience.
    build_dir = project_root / "build" / configure_preset
    src = build_dir / "compile_commands.json"
    dst = project_root / "compile_commands.json"
    if src.is_file():
        shutil.copy2(src, dst)
        print(f"  {dst}")

    return 0
