# Building DeckCAD

## 1) Prerequisites

| Tool    | Version   | Notes |
|---------|-----------|-------|
| CMake   | >= 3.20   | |
| Clang   | 20.1.8    | Apple Clang also works, but the presets point at Homebrew LLVM |
| Python  | >= 3.10   | Dawn's dependency fetcher and code generators need it |
| Ninja   | any       | |
| Git     | any       | |

On macOS the Xcode Command Line Tools are enough; a full Xcode install is not
required.

## 2) Submodules

```
git submodule update --init --recursive
```

This populates `submodules/` with:

| Submodule    | Role |
|--------------|------|
| `sdl`        | Window creation and input (SDL3) |
| `dawn`       | Native WebGPU implementation (Metal / D3D12 / Vulkan) |
| `imgui`      | Dear ImGui, `docking` branch, for the panel UI |
| `freetype`   | Font outline loading |
| `msdfgen`    | Multi-channel signed distance field generation |
| `nanosvg`    | SVG icon rasterization |
| `googletest` | Unit tests |
| `glaze`      | Reserved for scene serialization; not yet wired into the build |
| `tracy`      | Reserved for profiling; not yet wired into the build |

Dawn pulls its own third-party tree at CMake configure time
(`DAWN_FETCH_DEPENDENCIES=ON`), so `depot_tools` is not needed.

## 3) Compiler paths

Run the setup script, which installs the pinned compiler for your platform
(Homebrew on macOS, `apt`/`dnf` on Linux, `winget` on Windows) and writes
`CMakeUserEnv.json` for you:

```
python3 scripts/setup_dev_env.py
```

It reads `DECKCAD_TOOLCHAIN` to decide which compiler to install (format
`<compiler>-<version>`, e.g. `clang-20`; defaults to `clang-20` if unset).
Set it to pin a different version:

```
DECKCAD_TOOLCHAIN=clang-19 python3 scripts/setup_dev_env.py
```

Only `clang-<version>` is supported today, since the CMake config assumes a
Clang toolchain (Objective-C++ on macOS, some Clang-specific warning flags
elsewhere). On Windows, `winget` doesn't offer per-minor-version LLVM
packages, so the version there is best-effort -- the script installs latest
LLVM and reports what it actually got.

The script also installs `ccache` if it isn't already on `PATH` (see
[Shared build cache](#shared-build-cache) below), and creates
`DECKCAD_DEPS_DIR`.

If you'd rather do this by hand, create `CMakeUserEnv.json` in the project
root yourself (git-ignored):

```json
{
  "version": 9,
  "configurePresets": [
    {
      "name": "user_toolchain",
      "environment": {
        "DECKCAD_CC_PATH": "/path/to/clang",
        "DECKCAD_CXX_PATH": "/path/to/clang++"
      }
    }
  ]
}
```

## 4) Build

```
cmake --preset Debug
cmake --build --preset Debug
```

The first build compiles Dawn and takes roughly 10-20 minutes. Later builds
are incremental and fast.

Run the app:

```
./build/bin/DeckCAD
```

Run the tests:

```
ctest --preset Debug
```

A release build uses a separate directory so the two never fight over the
same Dawn artifacts:

```
cmake --preset Release && cmake --build --preset Release
```

### Hot reload

Configure with `DECKCAD_HOT_RELOAD=ON` to build `deckcad_core` (scenes, tools,
panels) as a shared library instead of linking it into `DeckCAD` directly:

```
cmake --preset Debug -DDECKCAD_HOT_RELOAD=ON
cmake --build --preset Debug
```

With the app running, rebuilding just `deckcad_core` — e.g.
`cmake --build build --target deckcad_core` — reloads it into the running
process on the next frame, so panel and app-logic edits show up without
restarting. Editing `Main`, `Graphics`, or `Platform` still needs a restart.

This also switches SDL3 to a shared library, so both the executable and
`deckcad_core` load the same copy instead of each getting their own
statically-linked one, which would otherwise register macOS's Objective-C
classes twice and crash.

### Shared build cache

The first build compiles Dawn and takes 10-20 minutes, and that cost repeats
for every new `build/` directory -- a fresh worktree, a wiped build dir, a
CI runner -- even though the source and flags haven't changed. A compiler
cache (`ccache`, or `sccache` if you prefer it) fixes this: `cmake/DeckCADDeps.cmake`
detects one on `PATH` and routes every compile through it automatically, with
its cache directory pinned under `DECKCAD_DEPS_DIR` rather than the ambient
environment, so it applies consistently regardless of how `ninja`/
`cmake --build` ends up invoked later.

`DECKCAD_DEPS_DIR` defaults to a per-OS cache location
(`~/Library/Caches/DeckCAD/deps` on macOS, `~/.cache/deckcad/deps` on Linux,
`%LOCALAPPDATA%\DeckCAD\deps` on Windows) and can be overridden with the
`DECKCAD_DEPS_DIR` environment variable. `scripts/setup_dev_env.py` installs
`ccache` and creates this directory for you; without it, builds still work,
just without the cross-worktree win. Disable it with
`-DDECKCAD_USE_COMPILER_CACHE=OFF` if it ever gets in the way.

**Known limitation:** this only shares *compiled objects*, not CMake's own
configure-time checks (Dawn and SDL each run a few dozen `try_compile`
probes on every fresh `build/` directory, adding tens of seconds regardless
of the compiler cache) or Dawn's dependency-fetch step. It also doesn't
attempt to synchronize two worktrees compiling the same file at the exact
same moment -- that's a normal, low-stakes cache race, not a correctness
issue.

## 5) Assets

`assets/` is staged next to the executable after every build. At runtime the
app resolves assets by walking up from the executable's own directory, so it
runs correctly from any working directory.

Shaders live in `assets/shaders/*.wgsl` and are loaded at runtime, so editing
one only requires re-running the build's asset staging step, not a recompile.

## 6) Optional: pre-commit formatting

```
pip install pre-commit
pre-commit install
```

The hook runs clang-format over staged files. `pre-commit run --all-files`
formats the whole tree.

## 7) Cross-compiling (experimental)

`Docker/Dockerfile` has two additional stages beyond the native Linux build
image, each pairing a cross-compiler with a CMake toolchain file under
`cmake/toolchains/`:

Run from the repo root, not `Docker/` -- the Dockerfile `COPY`s
`cmake/toolchains/` and `Docker/macos-sdk/`, both outside that directory:

```
docker build -f Docker/Dockerfile --target windows-cross -t deckcad-build-windows .
docker build -f Docker/Dockerfile --target macos-cross -t deckcad-build-macos .
```

(or `make -C Docker build-windows` / `make -C Docker build-macos`, which do the same thing)

- **Windows** uses [llvm-mingw](https://github.com/mstorsjo/llvm-mingw)
  (fully open-source, no SDK to supply) and
  `cmake/toolchains/windows-x86_64.cmake`.
- **macOS** uses [osxcross](https://github.com/tpoechtrager/osxcross) and
  `cmake/toolchains/macos-{arm64,x86_64}.cmake`. Apple's license doesn't
  allow redistributing its SDK, so you have to supply your own: see
  `Docker/macos-sdk/README.md`. `docker build --target macos-cross` fails
  clearly at the `COPY` step if it's missing.

**Honest scope:** both stages are verified only with a trivial C smoke test
baked into the Dockerfile (confirms the cross-compiler and sysroot produce a
binary for that platform). Cross-compiling all of DeckCAD, including Dawn,
has not been exercised end-to-end -- Dawn's own code generators (Tint) have
to run as host-native tools even while cross-compiling the final artifacts,
which is a known sharp edge for CMake projects with codegen steps, and
llvm-mingw's DirectX 12 headers lag the real Windows SDK, so Dawn's D3D12
backend is the likeliest thing to need extra work. Treat these as a starting
point for getting the toolchain plumbing right, not a guarantee that
`cmake --build` finishes on the first try.

To use a toolchain file directly (inside or outside the Docker image):

```
cmake -S . -B build-windows -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/windows-x86_64.cmake
```
