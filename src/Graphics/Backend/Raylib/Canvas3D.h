#pragma once
#include "Component.h"
#include "UiContext.h"
#include "UiTypes.h"
#include "raylib.h"

// A 3D viewport element (Raylib backend). Reserve a region in the UI layout, then
// the framework renders your raylib 3D scene into a RenderTexture sized to that
// region (so the camera aspect is correct) and composites it into the rect.
//
// Subclass it, set up `camera`, and override Draw3D():
//
//   class Viewport : public Ui::Raylib::Canvas3D {
//       void Draw3D(Ui::Rect) override { DrawGrid(20, 1.0f); DrawCube(...); }
//   };
//   static Viewport gViewport;                       // one long-lived instance
//   gViewport.camera.position = {...};               // drive the camera each frame
//   gViewport.Render(Ui::HashId("viewport", 0));     // place it in the layout
//   ...
//   gViewport.Unload();  // before CloseWindow
//
// Render/composite happen at dispatch with the final rect (no frame lag). Place the
// viewport as a panel-filling element, not inside a clipped/scrolling container
// (an active scissor is not suspended around the 3D render target).
namespace Ui::Raylib {

class Canvas3D : public Component {
public:
    Camera3D camera {
        Vector3 { 12.0f, 12.0f, 12.0f }, // position (isometric default)
        Vector3 { 0.0f, 0.0f, 0.0f }, // target
        Vector3 { 0.0f, 1.0f, 0.0f }, // up
        45.0f, // fovy
        CAMERA_PERSPECTIVE
    };
    UiColor background { 30, 30, 36, 255 };
    Rect lastRect {}; // rect of the last composite (window coords) - for ray picking.

    virtual ~Canvas3D() { Unload(); }

    // User 3D drawing (DrawGrid / DrawModel / DrawCube / ...), inside a
    // BeginMode3D(camera) already set up for `rect`.
    virtual void Draw3D(Rect rect) = 0;

    // Optional 2D overlay, drawn AFTER EndMode3D but still inside the render texture —
    // so it composites with the 3D scene and needs no separate layout element.
    //
    // This is where screen-space annotation goes: text, in particular, which has no 3D
    // primitive in raylib. Project with GetWorldToScreenEx(pos, camera, w, h) — pass the
    // TEXTURE's size (rect.w/rect.h), not the window's — then draw in those coordinates.
    //
    // Coordinates are the texture's own top-left-origin space, the same space Draw3D's
    // projection uses. The composite's vertical flip applies to the whole texture, so
    // 2D and 3D content stay consistent with each other and both land upright.
    virtual void Draw2D(Rect rect) { (void)rect; }

    // Reserve the region and register the deferred 3D render/composite.
    void Render(UiId id = kNullId, Sizing sizing = { Grow(), Grow() })
    {
        id_ = id != kNullId ? id : HashId(this, 0);
        LayoutConfig c {};
        c.sizing = sizing;
        c.hitTestable = true; // so the app can query Hovered() to drive the camera.
        u32 n = OpenElement(c, id_);
        ConfigureCustom(n, &Canvas3D::Trampoline, this);
        CloseElement();
        node_ = kNullIndex;
    }

    bool Hovered() const { return IsHovered(id_); }

    // Free the render texture. Call before CloseWindow (the GL context must be live).
    void Unload()
    {
        if (rt_.id != 0) {
            UnloadRenderTexture(rt_);
            rt_ = RenderTexture2D {};
        }
    }

private:
    RenderTexture2D rt_ {};

    static void Trampoline(void* user, Rect rect)
    {
        static_cast<Canvas3D*>(user)->Composite(rect);
    }

    void Composite(Rect rect)
    {
        lastRect = rect;
        // Skip collapsed / hidden regions entirely (no GPU work).
        if (rect.w < 1.0f || rect.h < 1.0f) {
            return;
        }
        // Round (not truncate) so a fractional layout doesn't distort the aspect or
        // thrash the texture by a sub-pixel each frame.
        int w = static_cast<int>(rect.w + 0.5f);
        int h = static_cast<int>(rect.h + 0.5f);

        // (Re)create the render texture when the region size changes.
        if (rt_.id == 0 || rt_.texture.width != w || rt_.texture.height != h) {
            if (rt_.id != 0) {
                UnloadRenderTexture(rt_);
            }
            rt_ = LoadRenderTexture(w, h); // single-sampled; window MSAA does not apply.
            SetTextureFilter(rt_.texture, TEXTURE_FILTER_BILINEAR);
        }
        if (rt_.id == 0) {
            return; // allocation failed (e.g. out of VRAM) - skip this frame safely.
        }

        BeginTextureMode(rt_);
        ClearBackground(Color { background.r, background.g, background.b, background.a });
        BeginMode3D(camera); // BeginMode3D takes aspect from the current FBO (the RT).
        Draw3D(rect);
        EndMode3D();
        Draw2D(rect); // screen-space overlay, still inside the render texture
        EndTextureMode();

        // Composite into the rect; flip vertically (FBO origin is bottom-left).
        DrawTexturePro(rt_.texture,
            Rectangle { 0, 0, static_cast<float>(rt_.texture.width), -static_cast<float>(rt_.texture.height) },
            Rectangle { rect.x, rect.y, rect.w, rect.h },
            Vector2 { 0, 0 }, 0.0f, Color { 255, 255, 255, 255 });
    }
};

} // namespace Ui::Raylib
