"""Platform detection and CMake preset resolution."""

from __future__ import annotations

import shutil
import sys
from pathlib import Path


def _is_ninja_available() -> bool:
    return shutil.which("ninja") is not None


def _detect_vs_version() -> int | None:
    """Return the major version of the latest Visual Studio, or None."""
    if sys.platform != "win32":
        return None

    import subprocess
    from pathlib import Path

    vswhere = (
        Path(r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe")
    )
    if not vswhere.exists():
        return None

    try:
        r = subprocess.run(
            [str(vswhere), "-latest", "-products", "*",
             "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
             "-property", "installationVersion"],
            capture_output=True, text=True, timeout=10,
        )
        if r.returncode == 0 and r.stdout.strip():
            major = r.stdout.strip().split(".")[0]
            return int(major)
    except Exception:
        pass
    return None


def detect_preset(mode: str, project_root: Path) -> tuple[str, str]:
    """Return (configure_preset, build_preset) for the current platform."""
    if sys.platform == "linux":
        return ("linux-release" if mode == "release" else "linux-debug",
                "linux-release" if mode == "release" else "linux-debug")

    if sys.platform == "darwin":
        return ("default-release" if mode == "release" else "default",
                "default-release" if mode == "release" else "default")

    if sys.platform == "win32":
        if _is_ninja_available():
            if mode == "release":
                return "windows-ninja-release", "windows-ninja-release"
            return "windows-ninja", "windows-ninja"

        vs = _detect_vs_version()
        if vs == 18:
            build = "windows-vs2026-release" if mode == "release" else "windows-vs2026-debug"
            return "windows-vs2026", build
        if vs == 17:
            build = "windows-vs2022-release" if mode == "release" else "windows-vs2022-debug"
            return "windows-vs2022", build

    print("[ERROR] No suitable generator found.", file=sys.stderr)
    print("        Install Ninja or Visual Studio (with C++ workload).", file=sys.stderr)
    sys.exit(1)


def detect_build_dir(mode: str, project_root: Path) -> Path:
    """Return the build directory for the given mode."""
    configure_preset, _ = detect_preset(mode, project_root)
    return project_root / "build" / configure_preset
