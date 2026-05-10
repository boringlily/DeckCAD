#!/usr/bin/env python3
"""Installs the pinned compiler and prepares the shared build cache.

Reads DECKCAD_TOOLCHAIN (format "<compiler>-<version>", e.g. "clang-20";
defaults to "clang-20") and installs that compiler with the platform's
package manager, then writes its resolved binary paths into
CMakeUserEnv.json (already git-ignored) so CMakePresets.json's "base" preset
picks them up via DECKCAD_CC_PATH / DECKCAD_CXX_PATH.

Also creates the shared cache directory (DECKCAD_DEPS_DIR, or its per-OS
default) that the compiler cache (see cmake/DeckCADDeps.cmake) and a
hand-installed ccache/sccache live under.

Usage:
    python3 scripts/setup_dev_env.py
    DECKCAD_TOOLCHAIN=clang-19 python3 scripts/setup_dev_env.py
"""

import json
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_TOOLCHAIN = "clang-20"


def run(command, **kwargs):
    print(f"$ {' '.join(command)}")
    return subprocess.run(command, check=True, **kwargs)


def parse_toolchain():
    spec = os.environ.get("DECKCAD_TOOLCHAIN", DEFAULT_TOOLCHAIN)
    if "-" not in spec:
        sys.exit(f"DECKCAD_TOOLCHAIN must look like '<compiler>-<version>' (e.g. 'clang-20'), got '{spec}'")
    compiler, version = spec.rsplit("-", 1)
    if compiler != "clang":
        sys.exit(f"Only 'clang-<version>' is supported today, got '{spec}'. "
                  "DeckCAD's CMakeLists.txt assumes a Clang toolchain (OBJCXX on macOS, warning flags elsewhere).")
    print(f"Toolchain: {spec}" + (" (default)" if spec == DEFAULT_TOOLCHAIN and "DECKCAD_TOOLCHAIN" not in os.environ else ""))
    return compiler, version


def install_macos(version):
    if not shutil.which("brew"):
        sys.exit("Homebrew not found. Install it from https://brew.sh, then re-run this script.")
    formula = f"llvm@{version}"
    run(["brew", "install", formula])
    prefix = subprocess.run(["brew", "--prefix", formula], check=True, capture_output=True, text=True).stdout.strip()
    cc = Path(prefix, "bin", "clang")
    cxx = Path(prefix, "bin", "clang++")
    if not cc.exists():
        sys.exit(f"Expected {cc} after installing {formula}, but it's not there.")
    return str(cc), str(cxx)


def install_linux(version):
    cc = shutil.which(f"clang-{version}")
    cxx = shutil.which(f"clang++-{version}")
    if cc and cxx:
        print(f"clang-{version} already installed.")
        return cc, cxx

    if shutil.which("apt-get"):
        run(["sudo", "apt-get", "update"])
        run(["sudo", "apt-get", "install", "-y", f"clang-{version}", f"lld-{version}"])
    elif shutil.which("dnf"):
        run(["sudo", "dnf", "install", "-y", f"clang-{version}", "lld"])
    else:
        sys.exit("Neither apt-get nor dnf found; install clang manually and set DECKCAD_CC_PATH/DECKCAD_CXX_PATH yourself.")

    cc = shutil.which(f"clang-{version}")
    cxx = shutil.which(f"clang++-{version}")
    if not cc or not cxx:
        installed = shutil.which("clang")
        sys.exit(
            f"Could not find clang-{version}/clang++-{version} on PATH after install "
            f"(this distro's repos may not carry that exact version"
            + (f"; it does have an unversioned 'clang' at {installed}" if installed else "")
            + "). Install it another way and set DECKCAD_CC_PATH/DECKCAD_CXX_PATH in CMakeUserEnv.json yourself."
        )
    return cc, cxx


def install_windows(version):
    if not shutil.which("winget"):
        sys.exit("winget not found. Install App Installer from the Microsoft Store, then re-run this script.")
    print(f"NOTE: winget does not offer per-minor-version LLVM packages, so the requested "
          f"version ({version}) is advisory only -- installing the latest LLVM release.")
    run(["winget", "install", "--id", "LLVM.LLVM", "-e", "--source", "winget"])

    program_files = os.environ.get("ProgramFiles", r"C:\Program Files")
    cc = Path(program_files, "LLVM", "bin", "clang.exe")
    cxx = Path(program_files, "LLVM", "bin", "clang++.exe")
    if not cc.exists():
        sys.exit(f"Expected {cc} after installing LLVM, but it's not there. "
                  "Find your install and set DECKCAD_CC_PATH/DECKCAD_CXX_PATH in CMakeUserEnv.json yourself.")
    print("Installed LLVM version (may differ from DECKCAD_TOOLCHAIN's request):")
    run([str(cc), "--version"])
    return str(cc), str(cxx)


def write_cmake_user_env(cc_path, cxx_path):
    path = REPO_ROOT / "CMakeUserEnv.json"
    document = {
        "version": 9,
        "configurePresets": [
            {
                "name": "user_toolchain",
                "environment": {
                    "DECKCAD_CC_PATH": cc_path,
                    "DECKCAD_CXX_PATH": cxx_path,
                },
            }
        ],
    }
    path.write_text(json.dumps(document, indent=2) + "\n")
    print(f"Wrote {path}")


def resolve_default_deps_dir():
    if "DECKCAD_DEPS_DIR" in os.environ:
        return Path(os.environ["DECKCAD_DEPS_DIR"])
    system = platform.system()
    home = Path.home()
    if system == "Windows":
        return Path(os.environ.get("LOCALAPPDATA", home / "AppData/Local")) / "DeckCAD" / "deps"
    if system == "Darwin":
        return home / "Library/Caches/DeckCAD/deps"
    xdg_cache = os.environ.get("XDG_CACHE_HOME")
    return Path(xdg_cache) / "deckcad" / "deps" if xdg_cache else home / ".cache/deckcad/deps"


def ensure_compiler_cache_installed():
    if shutil.which("ccache") or shutil.which("sccache"):
        print("Compiler cache: found on PATH already.")
        return
    system = platform.system()
    print("Compiler cache: ccache not found, attempting to install it (optional -- "
          "the build works without it, just without cross-worktree compile caching).")
    try:
        if system == "Darwin" and shutil.which("brew"):
            run(["brew", "install", "ccache"])
        elif system == "Linux" and shutil.which("apt-get"):
            run(["sudo", "apt-get", "install", "-y", "ccache"])
        elif system == "Linux" and shutil.which("dnf"):
            run(["sudo", "dnf", "install", "-y", "ccache"])
        elif system == "Windows" and shutil.which("winget"):
            run(["winget", "install", "--id", "ccache.ccache", "-e", "--source", "winget"])
        else:
            print("No known package manager found for ccache; skipping (install it yourself if you want it).")
    except subprocess.CalledProcessError:
        print("ccache install failed; continuing without it.")


def main():
    compiler, version = parse_toolchain()
    system = platform.system()

    if system == "Darwin":
        cc_path, cxx_path = install_macos(version)
    elif system == "Linux":
        cc_path, cxx_path = install_linux(version)
    elif system == "Windows":
        cc_path, cxx_path = install_windows(version)
    else:
        sys.exit(f"Unsupported platform: {system}")

    write_cmake_user_env(cc_path, cxx_path)

    deps_dir = resolve_default_deps_dir()
    deps_dir.mkdir(parents=True, exist_ok=True)
    print(f"Shared cache directory: {deps_dir}")

    ensure_compiler_cache_installed()

    print("\nDone. Next:")
    print("  cmake --preset Debug")
    print("  cmake --build --preset Debug")


if __name__ == "__main__":
    main()
