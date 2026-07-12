"""raiden build [--preset NAME] [--clean]"""

from __future__ import annotations

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
    sub.add_argument(
        "--clean",
        action="store_true",
        help="Remove the build directory before configuring",
    )
    sub.add_argument(
        "-D",
        dest="cmake_defines",
        action="append",
        default=[],
        metavar="VAR=VALUE",
        help="Extra CMake cache variable (repeatable, e.g. -DRAIDEN_USE_QT=ON)",
    )

def run_build(args, project_root: Path) -> int:
    build_dir = project_root / "build"

    if args.preset:
        configure_preset = args.preset
        build_preset = args.preset
    else:
        configure_preset, build_preset = detect_preset(args.mode, project_root)

    if args.clean:
        import shutil

        preset_dir = build_dir / configure_preset
        if preset_dir.is_dir():
            print(f"  Cleaning {preset_dir}")
            shutil.rmtree(preset_dir)

    print(f"  Configuring with preset: {configure_preset}")
    configure_cmd = ["cmake", "--preset", configure_preset]
    for define in args.cmake_defines:
        configure_cmd.append(f"-D{define}")

    r = subprocess.run(configure_cmd, cwd=project_root)
    if r.returncode != 0:
        return r.returncode

    print(f"  Building with preset: {build_preset}")
    r = subprocess.run(
        ["cmake", "--build", "--preset", build_preset],
        cwd=project_root,
    )
    return r.returncode
