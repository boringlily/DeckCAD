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

- **Common** — `Types.h`, `DcadMath.h`. 
- **Platform** — the only module that includes SDL. Resolves the asset paths
  relative to the executable.
- **Graphics** — `Gpu` (device, surface, frame), `Text` (MSDF atlas and
  renderer), `Ui` (ImGui context, theme, icons, widgets), `Viewport` (camera and
  the offscreen 3D scene target).
- **Core** — scenes, toolbox state and the ImGui panels.
- **Main** — Wire everything together and owns shutdown ordering.

## Conventions

### Core owns no GPU state

`Core` receives renderers through `FrameContext` and never stores them.
Anything holding a `wgpu::` handle belongs in `Graphics`.

### Hot reload

With `DECKCAD_HOT_RELOAD=ON`, `deckcad_core` builds as a shared library
(`Core/CMakeLists.txt`) instead of linking into `DeckCAD`. `Main` loads it
through `Platform::DynamicLibrary` via the thin `HotReloadCore` wrapper,
and reloads it whenever its file on disk changes allowing for hotreloading of panel or app-logic after edits.

`Core/CoreApi.h` declares the two `extern "C"` entry points Main calls
(`CoreAppInit`, `CoreBuildFrame`) — `extern "C"` avoids C++ name mangling so
they can be looked up by plain name. 
`AppState` Is owned by Main so it survives a reload. 

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
  from screen-space derivatives.
- `msdf_text.wgsl` — MSDF glyph quads anchored in world space but sized in
  pixels, with the edge width derived from `fwidth` such that it stays one pixel wide
  under any projection.
- `solid.wgsl` — flat vertex-coloured triangles; currently used for the origin planes.

All three emit premultiplied alpha and blend with `One / OneMinusSrcAlpha`.

## Viewport controls

| Input                     | Action |
|---------------------------|--------|
| Left + Right mouse drag   | Orbit  |
| Middle mouse drag         | Pan    |
| Wheel                     | Zoom   |

Zoom is exponential, so a notch feels the same at every scale, and distance is
clamped at both ends.

## Future work: 

`glaze` submodule was added for future scene serialization and project data serializion.  
`tracy` Profiler is also part of the submodules but has not been wired up into this new stack. 
