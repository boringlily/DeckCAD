#!/usr/bin/env python3
"""Runs clang-tidy over DeckCAD's own sources (not submodules).

Reuses an existing configure's compile_commands.json rather than doing its
own build, so this is quick to run repeatedly and needs no sanitizer/tidy-
specific CMake setup of its own.

Usage:
    cmake --preset Debug   # if not already configured
    python3 scripts/run_clang_tidy.py
    python3 scripts/run_clang_tidy.py --build-dir build-release
    python3 scripts/run_clang_tidy.py --fix   # apply suggested fixes in place
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
SOURCE_DIRS = ["src", "tests"]


def find_run_clang_tidy():
    for name in ("run-clang-tidy", "run-clang-tidy.py"):
        found = shutil.which(name)
        if found:
            return found
    sys.exit(
        "run-clang-tidy not found on PATH. It ships with LLVM/Clang "
        "(e.g. Homebrew's llvm@20 formula, or your Linux distro's "
        "clang-tools-extra package) but isn't always symlinked onto PATH -- "
        "add its bin directory, or run scripts/setup_dev_env.py."
    )


def collect_source_files():
    files = []
    for source_dir in SOURCE_DIRS:
        files += [str(p) for p in (REPO_ROOT / source_dir).rglob("*.cpp")]
    if not files:
        sys.exit(f"No .cpp files found under {SOURCE_DIRS} -- run this from the repo, or check the paths.")
    return files


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", default="build", help="Directory with compile_commands.json (default: build)")
    parser.add_argument("--fix", action="store_true", help="Apply clang-tidy's suggested fixes in place")
    parser.add_argument("--jobs", "-j", type=int, default=None, help="Parallel jobs (default: run-clang-tidy's own default, one per core)")
    args = parser.parse_args()

    build_dir = (REPO_ROOT / args.build_dir).resolve()
    if not (build_dir / "compile_commands.json").exists():
        sys.exit(
            f"No compile_commands.json in {build_dir}. Configure first, e.g.:\n"
            f"  cmake --preset Debug"
        )

    run_clang_tidy = find_run_clang_tidy()
    # .clang-tidy itself leaves WarningsAsErrors empty, so IDE integrations
    # and ad-hoc `clang-tidy` runs just show warnings without failing.
    # -warnings-as-errors here is what makes THIS script (local or CI) an
    # actual gate instead of a report nobody has to act on.
    command = [run_clang_tidy, "-p", str(build_dir), "-quiet", "-warnings-as-errors=*"]
    if args.jobs:
        command += ["-j", str(args.jobs)]
    if args.fix:
        command += ["-fix"]
    command += collect_source_files()

    result = subprocess.run(command, cwd=REPO_ROOT)
    sys.exit(result.returncode)


if __name__ == "__main__":
    main()
