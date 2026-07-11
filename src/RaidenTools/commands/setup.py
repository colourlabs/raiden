"""raiden setup"""

from __future__ import annotations

import platform
import shutil
import sys
import tarfile
import urllib.request

from pathlib import Path

SLANG_VERSION = "2026.12.2"
KTX_VERSION = "4.4.2"

def add_arguments(sub):
    sub.add_argument(
        "--check",
        action="store_true",
        help="Only check prerequisites, don't download tools",
    )


def _check_cmd(name: str, hint: str) -> bool:
    if shutil.which(name):
        print(f"  [OK]   {name} found")
        return True
    print(f"  [FAIL] {name} not found - {hint}")
    return False

def _detect_platform() -> str:
    s = platform.system().lower()
    if s == "linux":
        return "linux-x86_64"
    if s == "darwin":
        m = platform.machine().lower()
        return "macos-arm64" if m == "arm64" else "macos-x86_64"
    if s == "windows":
        return "windows-x86_64"
    return s


def _download(url: str, dest: Path):
    print(f"  Downloading {url.split('/')[-1]}...")
    urllib.request.urlretrieve(url, dest)

def _install_slang(tools_dir: Path, bin_dir: Path, plat: str) -> bool:
    if (bin_dir / "slangc").is_file():
        print("  [OK]   slangc already present")
        return True

    if plat == "linux-x86_64":
        url = (f"https://github.com/shader-slang/slang/releases/download/"
               f"v{SLANG_VERSION}/slang-{SLANG_VERSION}-linux-x86_64.tar.gz")
    elif plat.startswith("macos"):
        url = (f"https://github.com/shader-slang/slang/releases/download/"
               f"v{SLANG_VERSION}/slang-{SLANG_VERSION}-{plat}.tar.gz")
    elif plat == "windows-x86_64":
        url = (f"https://github.com/shader-slang/slang/releases/download/"
               f"v{SLANG_VERSION}/slang-{SLANG_VERSION}-windows-x86_64.zip")
    else:
        print(f"  [SKIP] No slang binary available for {plat}")
        return False

    archive = tools_dir / ("slang.tar.gz" if "tar" in url else "slang.zip")
    try:
        _download(url, archive)
        print("  Extracting...")
        if archive.suffix == ".gz":
            with tarfile.open(archive) as tf:
                tf.extractall(tools_dir)
        else:
            import zipfile
            with zipfile.ZipFile(archive) as zf:
                zf.extractall(tools_dir)

        # find slangc binary
        slangc_name = "slangc.exe" if sys.platform == "win32" else "slangc"
        for candidate in tools_dir.rglob(slangc_name):
            shutil.copy2(candidate, bin_dir / slangc_name)
            if sys.platform != "win32":
                (bin_dir / slangc_name).chmod(0o755)
            print(f"  [OK]   slangc installed to {bin_dir}")
            return True

        print("  [FAIL] Could not find slangc after extraction")
        return False
    finally:
        archive.unlink(missing_ok=True)


def _install_ktx(tools_dir: Path, bin_dir: Path, plat: str) -> bool:
    if (bin_dir / "toktx").is_file():
        print("  [OK]   toktx already present")
        return True

    if plat == "linux-x86_64":
        url = (f"https://github.com/KhronosGroup/KTX-Software/releases/download/"
               f"v{KTX_VERSION}/KTX-Software-{KTX_VERSION}-Linux-x86_64.tar.bz2")
    elif plat.startswith("macos"):
        url = (f"https://github.com/KhronosGroup/KTX-Software/releases/download/"
               f"v{KTX_VERSION}/KTX-Software-{KTX_VERSION}-{plat}.tar.bz2")
    elif plat == "windows-x86_64":
        url = (f"https://github.com/KhronosGroup/KTX-Software/releases/download/"
               f"v{KTX_VERSION}/KTX-Software-{KTX_VERSION}-Windows-x64.zip")
    else:
        print(f"  [SKIP] No toktx binary available for {plat}")
        return False

    archive = tools_dir / ("ktx.tar.bz2" if "tar" in url else "ktx.zip")
    try:
        _download(url, archive)
        print("  Extracting...")
        extract_dir = tools_dir / "ktx"
        if archive.suffix == ".gz" or archive.name.endswith(".bz2"):
            with tarfile.open(archive) as tf:
                tf.extractall(extract_dir)
        else:
            import zipfile
            with zipfile.ZipFile(archive) as zf:
                zf.extractall(extract_dir)

        toktx_name = "toktx.exe" if sys.platform == "win32" else "toktx"
        for candidate in extract_dir.rglob(toktx_name):
            shutil.copy2(candidate, bin_dir / toktx_name)
            if sys.platform != "win32":
                (bin_dir / toktx_name).chmod(0o755)
            print(f"  [OK]   toktx installed to {bin_dir}")
            return True

        print("  [FAIL] Could not find toktx after extraction")
        return False
    finally:
        archive.unlink(missing_ok=True)


def run_setup(args, project_root: Path) -> int:
    tools_dir = project_root / "tools"
    bin_dir = tools_dir / "bin"
    bin_dir.mkdir(parents=True, exist_ok=True)

    plat = _detect_platform()
    print(f"raiden tools setup ({plat})")
    print("=" * 40)
    print()

    # prerequisites
    print("[1/3] Checking prerequisites...")
    ok = True
    ok = _check_cmd("cmake", "Install from https://cmake.org/download/") or ok
    ok = _check_cmd("git", "Install from https://git-scm.com/downloads") or ok
    if sys.platform != "win32":
        ok = _check_cmd("g++", "Install g++ or clang++ from your package manager") or ok
    if not args.check and not ok:
        print("\nFix missing prerequisites before continuing.")
        return 1

    if args.check:
        slang_ok = (bin_dir / "slangc").is_file()
        toktx_ok = (bin_dir / "toktx").is_file()
        print(f"\n  slangc: {'[OK]' if slang_ok else '[MISS]'}")
        print(f"  toktx:  {'[OK]' if toktx_ok else '[MISS]'}")
        return 0 if (slang_ok and toktx_ok) else 1

    # tools
    print("\n[2/3] Downloading tools...")
    _install_slang(tools_dir, bin_dir, plat)
    _install_ktx(tools_dir, bin_dir, plat)

    # summary
    print("\n[3/3] Summary")
    print(f"  slangc: {'[OK]' if (bin_dir / 'slangc').is_file() else '[MISS]'}")
    print(f"  toktx:  {'[OK]' if (bin_dir / 'toktx').is_file() else '[MISS]'}")
    print(f"\n  Tools directory: {bin_dir}")
    print(f"  Add to PATH: export PATH=\"$PATH:{bin_dir}\"")
    return 0
