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

Create `CMakeUserEnv.json` in the project root (git-ignored):

```json
{
  "version": 9,
  "configurePresets": [
    {
      "name": "user_clang_paths",
      "environment": {
        "USER_CLANG_20_PATH": "/path/to/clang",
        "USER_CLANG++_20_PATH": "/path/to/clang++"
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
