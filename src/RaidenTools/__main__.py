"""raiden - raiden engine developer tools."""

from __future__ import annotations

import sys
from pathlib import Path

from RaidenTools.commands.build import add_arguments as add_build_args, run_build
from RaidenTools.commands.compiledb import add_arguments as add_compiledb_args, run_compiledb
from RaidenTools.commands.editor import add_arguments as add_editor_args, run_editor
from RaidenTools.commands.run import add_arguments as add_run_args, run_game
from RaidenTools.commands.setup import add_arguments as add_setup_args, run_setup


def _find_project_root() -> Path:
    """Walk up from CWD to find CMakeLists.txt with the raiden project."""
    d = Path.cwd()
    for _ in range(10):
        if (d / "CMakeLists.txt").is_file():
            content = (d / "CMakeLists.txt").read_text(errors="ignore")
            if "project(raiden" in content:
                return d
        parent = d.parent
        if parent == d:
            break
        d = parent
    print("[ERROR] Could not find raiden project root.", file=sys.stderr)
    print("        Run this command from within the raiden repository.", file=sys.stderr)
    sys.exit(1)


def main() -> int:
    import argparse

    parser = argparse.ArgumentParser(
        prog="raiden",
        description="raiden engine developer tools",
    )
    sub = parser.add_subparsers(dest="command")

    p_build = sub.add_parser("build", help="Configure and build the engine")
    add_build_args(p_build)

    p_run = sub.add_parser("run", help="Run the engine or editor")
    add_run_args(p_run)

    p_editor = sub.add_parser("editor", help="Launch the editor standalone")
    add_editor_args(p_editor)

    p_setup = sub.add_parser("setup", help="Download required tools (slangc, toktx)")
    add_setup_args(p_setup)

    p_compiledb = sub.add_parser("compiledb", help="Regenerate compile_commands.json")
    add_compiledb_args(p_compiledb)

    args = parser.parse_args()
    if args.command is None:
        parser.print_help()
        return 0

    root = _find_project_root()

    if args.command == "build":
        return run_build(args, root)
    if args.command == "run":
        return run_game(args, root)
    if args.command == "editor":
        return run_editor(args, root)
    if args.command == "setup":
        return run_setup(args, root)
    if args.command == "compiledb":
        return run_compiledb(args, root)

    parser.print_help()
    return 0


if __name__ == "__main__":
    sys.exit(main())
