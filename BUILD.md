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
(Homebrew on macOS, `apt`/`dnf` on Linux, `winget` on Windows) and exports its
resolved path as real, persistent environment variables --
`DECKCAD_CC_PATH` / `DECKCAD_CXX_PATH`, appended to your shell rc file on
macOS/Linux, via `setx` on Windows:

```
python3 scripts/setup_dev_env.py
```

`cmake/toolchains/host.cmake` reads those two variables and is wired in as
`CMakePresets.json`'s toolchain file, so there's no per-clone preset file to
author or keep in sync -- once the script has run on a machine, every
worktree on it picks up the same compiler automatically. Open a new shell
(or `source` your rc file) after running the script, so the variables are
actually in your environment before you configure.

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
[Compiler cache](#compiler-cache) below), and creates
`DECKCAD_DEPS_DIR`.

If you'd rather do this by hand, just export the two variables yourself,
however you normally manage your shell environment:

```
export DECKCAD_CC_PATH=/path/to/clang
export DECKCAD_CXX_PATH=/path/to/clang++
```

If neither is set, `host.cmake` warns and falls back to CMake's default
compiler detection rather than failing the configure.

## 4) Build

```
cmake --preset Debug
cmake --build --preset Debug
```

The first time this runs *on a machine* -- not per clone, per worktree, or
per wiped `build/` directory -- it builds and installs SDL, Dawn, FreeType,
msdfgen and googletest into a shared cache directory, which is where the
10-20 minutes (mostly Dawn) goes. Every subsequent `cmake --preset Debug` on
that machine, from any worktree, for that same submodule commit, just
`find_package()`s the already-installed result: no reconfigure, no rebuild.
See [External dependencies](#external-dependencies) below for how.

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

### External dependencies

SDL, Dawn, FreeType, msdfgen and googletest each build as an independent,
installed CMake project in a shared cache directory (`DECKCAD_DEPS_DIR`, see
below) instead of being compiled straight into DeckCAD's own `build/`
directory the way `imgui` and `nanosvg` still are. `cmake/DeckCADDeps.cmake`'s
`deckcad_build_external_dependency()` does this for each one: it works out
the submodule's currently-checked-out commit, and if
`<DECKCAD_DEPS_DIR>/<name>-<build-type>-<commit>/.deckcad-installed` doesn't exist yet, it
configures, builds and installs that submodule as its own `cmake -S/-B`
project (synchronously, as part of *this* project's own configure step --
not CMake's `ExternalProject_Add`, which only runs at build time and can't
finish before `find_package()` needs the result). Either way, the surrounding
`CMakeLists.txt` then just does an ordinary `find_package(... CONFIG REQUIRED
PATHS <that install dir> NO_DEFAULT_PATH)` and links the same target names as
before (`SDL3::SDL3`, `dawn::webgpu_dawn`, `msdfgen::msdfgen-core`, ...).

This is genuinely a different, separate build from DeckCAD's own -- worth
knowing two consequences of that:

- **A cold start on a new machine is slower in wall-clock time than the old
  single-Ninja-graph build was**, not faster: SDL, Dawn, FreeType and msdfgen
  now build one after another (each is its own sequential `execute_process`
  during configure) instead of all four sharing every CPU core at once the
  way one unified `add_subdirectory()` graph let them. The payoff is that
  this cost is paid once per (machine, submodule commit) instead of once per
  worktree/build-dir -- which is the actual goal, cold start aside.
- **Two worktrees racing to build the same not-yet-cached commit at the same
  time will corrupt each other's build** -- there's no lock file, just a
  stamp written at the end. In practice this only matters the very first time
  a submodule commit is ever built on a machine; avoid kicking off two clean
  builds simultaneously on a brand new machine/CI image.

`DECKCAD_DEPS_DIR` defaults to a per-OS cache location
(`~/Library/Caches/DeckCAD/deps` on macOS, `~/.cache/deckcad/deps` on Linux,
`%LOCALAPPDATA%\DeckCAD\deps` on Windows) and can be overridden with the
`DECKCAD_DEPS_DIR` environment variable. To force a clean rebuild of one
dependency, delete its directory under there (e.g.
`rm -rf $DECKCAD_DEPS_DIR/dawn-debug-<commit>`) and reconfigure.

Two things are keyed into the cache path beyond just name + commit, because
they change what actually gets built: SDL is cached as `sdl-static-<build-type>-<commit>`
or `sdl-shared-<build-type>-<commit>` depending on `DECKCAD_HOT_RELOAD` (see below) --
otherwise toggling that option on a machine that already built the other
linkage would silently reuse a wrongly-linked install.

`imgui` and `nanosvg` stay compiled directly into `build/` rather than
externalized: neither ships its own CMake project with install support
(`cmake/imgui/CMakeLists.txt` is ours, not upstream's), and both are fast
enough to compile that there's no real cold-start cost to share.

### Compiler cache

On top of the above, this project's *own* code (plus in-tree `imgui`) also
routes through a compiler cache (`ccache`, or `sccache` if you prefer it) when
one is on `PATH`, cache directory pinned under `DECKCAD_DEPS_DIR/compiler-cache`.
`scripts/setup_dev_env.py` installs `ccache` for you. Disable it with
`-DDECKCAD_USE_COMPILER_CACHE=OFF` if it ever gets in the way. This is a much
smaller win than the external-dependency installs above -- it just makes
recompiling DeckCAD's own sources across build directories faster, the same
way it would for any project.

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

The hook runs clang-format over staged files, using the style in `.clang-format`
(`WebKit`, with include-sorting off since include order here is deliberate,
not alphabetical). `pre-commit run --all-files` formats the whole tree.
Format it by hand instead with:

```
clang-format -i $(find src tests -name '*.cpp' -o -name '*.h')
```

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

## 8) Sanity checks

Three independent checks, all runnable locally with a single command each
and wired into `.github/workflows/ci.yml` so they run the same way in CI:

| Check | Local command | Catches |
|-------|----------------|---------|
| Format | `pre-commit run --all-files` (or `clang-format -i ...`, see above) | Style, not caught by review |
| Naming | `python3 scripts/run_clang_tidy.py` | The naming conventions clang-format can't check (see `.clang-tidy`), plus a small set of bug-prone patterns |
| Sanitizers | `cmake --preset Sanitize && cmake --build --preset Sanitize && ctest --preset Sanitize` | Memory errors (ASan) and undefined behavior (UBSan) in DeckCAD's own code |

### clang-tidy

`.clang-tidy` enforces the naming half of the project's C++ style rules that
clang-format has no opinion on -- free functions in `PascalCase`, methods in
`camelBack`, variables in `lower_snake`, private/protected members with a
trailing `_`, namespace-scope named constants in `UPPER_CASE` -- plus a small,
deliberately conservative set of bug-prone/performance checks. It does not
(can't, really) enforce everything the style calls for: pointer/reference
suffixes like `_ptr`/`_ref`, the "no abbreviations" rule, and "methods are at
least two words" stay a manual-review concern.

`scripts/run_clang_tidy.py` runs it over `src/` and `tests/` only (never the
submodules) against an already-configured build's `compile_commands.json`,
with `-warnings-as-errors=*` so it actually fails instead of just printing --
`.clang-tidy` itself leaves `WarningsAsErrors` empty so IDE integrations and
ad-hoc `clang-tidy` invocations still just show warnings without failing
anything. Needs `cmake --preset Debug` to have run at least once first.

### Sanitizers

`DECKCAD_ENABLE_ASAN` / `DECKCAD_ENABLE_UBSAN` (both `ON` in the `Sanitize`
preset, both individually available via `-D` on any preset) add
`-fsanitize=...` to DeckCAD's own targets and in-tree `imgui` --
deliberately **not** to the externally-built SDL/Dawn/FreeType/msdfgen/
googletest (see [External dependencies](#external-dependencies)): those are
separate, already-installed builds, and instrumenting them too would mean a
second full Dawn build just to sanitize code this project doesn't own.
AddressSanitizer still catches memory bugs at the boundary with those
libraries (it intercepts `malloc`/`free` globally); UndefinedBehaviorSanitizer
issues *inside* their own code specifically won't be caught, only in ours.

```
cmake --preset Sanitize
cmake --build --preset Sanitize
ctest --preset Sanitize
```

**Known issue on macOS:** as of writing, AddressSanitizer's own runtime
initialization deadlocks on at least one recent macOS version, for both
Apple Clang and Homebrew LLVM 20 -- confirmed by sampling a hung process: it
never reaches `main()`, stuck in AddressSanitizer's own startup code before
any of this project's code runs. `MallocNanoZone=0` and
`DYLD_SHARED_REGION=avoid` (both otherwise-known workarounds for
ASan-on-macOS issues) don't fix it. UBSan alone (`-DDECKCAD_ENABLE_ASAN=OFF
-DDECKCAD_ENABLE_UBSAN=ON`) is unaffected and was verified working, tests and
all. If `ctest --preset Sanitize` hangs on your Mac, try that combination
locally and lean on Linux CI for full ASan+UBSan coverage in the meantime.

### CI

`.github/workflows/ci.yml` runs three jobs on every push/PR: `build-debug`
(the `Debug` preset), `sanitize` (the `Sanitize` preset), and `lint`
(clang-format + clang-tidy). `sanitize` and `lint` both `needs: build-debug`
-- not for ordering's own sake, but because two jobs building the same
not-yet-cached dependency commit at the same time would race and corrupt
each other's install (the same caveat as two local worktrees doing it, see
[External dependencies](#external-dependencies)); by the time `sanitize`/
`lint` start, `build-debug` has already warmed the shared dependency cache
(restored via `actions/cache`, keyed by `git submodule status`), so neither
rebuilds SDL/Dawn/FreeType/msdfgen/googletest a second time.
