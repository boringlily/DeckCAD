# Architecture

## Stack

| Concern            | Library |
|--------------------|---------|
| Window and input   | SDL3 |
| Graphics           | WebGPU via Dawn (Metal on macOS, D3D12 on Windows, Vulkan on Linux) |
| UI                 | Dear ImGui (docking branch) |
| Viewport text      | FreeType + msdfgen |
| Icons              | nanosvg |
| Tests              | GoogleTest |

## Module layout

Modules depend strictly downward; nothing below reaches back up.

```
Main         entry point, frame loop, teardown order
  |
Core         application logic: scenes, tools, panels        (no GPU handles)
  |
Graphics     Gpu / Text / Ui / Viewport                      (owns all GPU state)
  |
Platform     SDL window, event pump, asset resolution
  |
Common       scalar types, linear algebra                    (header-only)
```

- **Common** — `Types.h`, `DeckMath.h`. No dependencies.
  Named `DeckMath.h` rather than `Math.h` on purpose: macOS filesystems are
  case-insensitive, so a header called `Math.h` on the include path gets picked
  up by libc++'s `<cmath>` when it asks for `<math.h>`, and the standard library
  stops compiling.
- **Platform** — the only module that includes SDL. Also resolves asset paths
  relative to the executable.
- **Graphics** — `Gpu` (device, surface, frame), `Text` (MSDF atlas and
  renderer), `Ui` (ImGui context, theme, icons, widgets), `Viewport` (camera and
  the offscreen 3D scene target).
- **Core** — scenes, toolbox state and the ImGui panels.
- **Main** — wires it together and owns shutdown ordering.

## Conventions

### WebGPU depth range

Clip-space depth is `[0, 1]`, not OpenGL's `[-1, 1]`. Every projection in
`DeckMath.h` maps near to 0 and far to 1, depth attachments clear to `1.0`, and
`MathTests.cpp` guards the convention so a regression fails a test rather than
producing subtly wrong occlusion.

Matrices are column-major, matching WGSL's `mat4x4<f32>`, so they upload without
transposing.

### Core owns no GPU state

`Core` receives renderers through `FrameContext` and never stores them.
Anything holding a `wgpu::` handle belongs in `Graphics`.

### Hot reload

With `DECKCAD_HOT_RELOAD=ON`, `deckcad_core` builds as a shared library
(`Core/CMakeLists.txt`) instead of linking into `DeckCAD`. `Main` loads it
through `Platform::DynamicLibrary` (`Platform/DynamicLibrary.h`) via the thin
`HotReloadCore` wrapper (`Main/HotReloadCore.h`), and reloads it whenever its
file on disk changes — no restart needed to see a panel or app-logic edit.

`Core/CoreApi.h` declares the two `extern "C"` entry points Main calls
(`CoreAppInit`, `CoreBuildFrame`) — `extern "C"` avoids C++ name mangling so
they can be looked up by plain name. `AppState` survives a reload because
Main owns it, not Core, and Core never stores GPU handles in it.

Two things make this trickier than it looks:

- **Dear ImGui's global state doesn't cross the shared-library boundary.**
  `imgui_core` (widget drawing) is compiled once into the executable and
  again into `deckcad_core.dylib`, so each has its own independent, initially
  null `GImGui`. Both `CoreAppInit` and `CoreBuildFrame` take the caller's
  `ImGuiContext*` and call `ImGui::SetCurrentContext()` first — see Dear
  ImGui's own guidance on using it across DLL boundaries.
- **SDL3 must be shared, not static, in this configuration.** `imgui` (the
  SDL3/WebGPU backend target) is a separate target from `imgui_core`
  specifically so Core never needs it, but Core still needs `Graphics` for
  real GPU calls (`Viewport::beginScene()`, `renderFrame()`, ...), which pulls
  the rest of `Graphics`'s link dependencies — including SDL3 — into
  `deckcad_core.dylib`. Statically linking the same SDL3 archive into both the
  executable and the dylib registers macOS's Objective-C classes twice, which
  crashes; building SDL3 shared means both images load the same one.

### Frame ordering

```
PumpEvents
  -> gpu.BeginFrame()          acquire backbuffer, open the command encoder
  -> ui.BeginFrame()
  -> Core::BuildFrame()        builds the UI *and* records the offscreen
                               viewport pass into the same encoder
  -> UI render pass            ImGui draws, sampling the viewport texture
  -> gpu.EndFrame()            submit + present
```

The viewport pass is recorded before the UI pass in the same encoder, so the
texture is always written before ImGui samples it.

### Viewport render target

The viewport allocates its colour and depth textures rounded up to a 128 px
granularity and renders the scene into a sub-rect, exposing the used fraction as
a UV range. ImGui's WebGPU backend permanently caches a bind group per texture
view it encounters, so reallocating on every pixel of a splitter drag would leak
a texture per frame.

### Shaders

WGSL sources live in `assets/shaders/` and are loaded at runtime.

- `grid.wgsl` — analytic infinite grid. One full-screen triangle; each pixel
  intersects its view ray with the Y=0 plane and derives anti-aliased grid lines
  from screen-space derivatives, writing true depth so geometry occludes it.
- `msdf_text.wgsl` — MSDF glyph quads anchored in world space but sized in
  pixels, with the edge width derived from `fwidth` so it stays one pixel wide
  under any projection.
- `solid.wgsl` — flat vertex-coloured triangles; currently the origin planes,
  and the seam where solid modelling geometry will plug in.

All three emit premultiplied alpha and blend with `One / OneMinusSrcAlpha`.

## Viewport controls

Carried over from the original canvas:

| Input                     | Action |
|---------------------------|--------|
| Left + Right mouse drag   | Orbit  |
| Middle mouse drag         | Pan    |
| Wheel                     | Zoom   |

Zoom is exponential, so a notch feels the same at every scale, and distance is
clamped at both ends.

## Not yet wired in

`glaze` (scene serialization) and `tracy` (profiling) are checked out as
submodules but are not part of the build.
