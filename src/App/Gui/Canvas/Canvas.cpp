
#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include "rcamera.h"

#include <array>
#include <vector>
#include <string>
#include <sstream>
#include <optional>

#include "Components.h"
#include "Geometry.h"
#include "SketchFeature.h"
#include "CreateSketchCommand.h"

#include "CanvasPlanes.cpp"

static Clay_ElementId canvasId = CLAY_SID(Clay_String("CanvasPanel"));

// Half-extent of the rendered (and hittable) origin plane quads.
static constexpr float ORIGIN_PLANE_EXTENT { 5.0f };
static constexpr Vector2 ORIGIN_PLANE_SIZE { 10.0f, 10.0f };

// Per-axis tint colors for the three origin planes.
static constexpr Color PLANE_COLOR_XY { 50, 100, 230, 50 }; // blue  (Z normal)
static constexpr Color PLANE_COLOR_XZ { 50, 200, 80, 50 }; // green (Y normal)
static constexpr Color PLANE_COLOR_YZ { 230, 60, 60, 50 }; // red   (X normal)
// Highlight when hovering in plane-selection mode.
static constexpr Color PLANE_COLOR_HOVER { 255, 200, 50, 160 };
// Dimmed tint for non-hovered planes in selection mode.
static constexpr Color PLANE_COLOR_XY_DIM { 50, 100, 230, 30 };
static constexpr Color PLANE_COLOR_XZ_DIM { 50, 200, 80, 30 };
static constexpr Color PLANE_COLOR_YZ_DIM { 230, 60, 60, 30 };

// ──────────────────────────────────────────────────────────────────────────────
// Helpers

static Vector3 SketchPointToWorld(Geometry::Point2 p, Geometry::SketchPlane plane)
{
    switch (plane) {
    case Geometry::SketchPlane::XY:
        return { (float)p.x, (float)p.y, 0.0f };
    case Geometry::SketchPlane::XZ:
        return { (float)p.x, 0.0f, (float)p.y };
    case Geometry::SketchPlane::YZ:
        return { 0.0f, (float)p.x, (float)p.y };
    }
    return { 0, 0, 0 };
}

static UI::OriginPlane SketchPlaneToOriginPlane(Geometry::SketchPlane plane)
{
    switch (plane) {
    case Geometry::SketchPlane::XY:
        return UI::OriginPlane::XY;
    case Geometry::SketchPlane::XZ:
        return UI::OriginPlane::XZ;
    case Geometry::SketchPlane::YZ:
        return UI::OriginPlane::YZ;
    }
    return UI::OriginPlane::XY;
}

// Ray-vs-three-axis-planes test. Returns the closest plane hit within half_extent,
// or nullopt if none is hit.
static std::optional<Geometry::SketchPlane> ComputeHoveredOriginPlane(Ray ray, float half_extent)
{
    float best_t = 1e30f;
    std::optional<Geometry::SketchPlane> result;

    // XY plane (z = 0)
    if (fabsf(ray.direction.z) > 1e-6f) {
        float t = -ray.position.z / ray.direction.z;
        if (t > 0.0f && t < best_t) {
            float x = ray.position.x + t * ray.direction.x;
            float y = ray.position.y + t * ray.direction.y;
            if (fabsf(x) <= half_extent && fabsf(y) <= half_extent) {
                best_t = t;
                result = Geometry::SketchPlane::XY;
            }
        }
    }

    // XZ plane (y = 0)
    if (fabsf(ray.direction.y) > 1e-6f) {
        float t = -ray.position.y / ray.direction.y;
        if (t > 0.0f && t < best_t) {
            float x = ray.position.x + t * ray.direction.x;
            float z = ray.position.z + t * ray.direction.z;
            if (fabsf(x) <= half_extent && fabsf(z) <= half_extent) {
                best_t = t;
                result = Geometry::SketchPlane::XZ;
            }
        }
    }

    // YZ plane (x = 0)
    if (fabsf(ray.direction.x) > 1e-6f) {
        float t = -ray.position.x / ray.direction.x;
        if (t > 0.0f && t < best_t) {
            float y = ray.position.y + t * ray.direction.y;
            float z = ray.position.z + t * ray.direction.z;
            if (fabsf(y) <= half_extent && fabsf(z) <= half_extent) {
                best_t = t;
                result = Geometry::SketchPlane::YZ;
            }
        }
    }

    return result;
}

// ──────────────────────────────────────────────────────────────────────────────
// Per-frame interaction state lives on the Scene (was_sketch_active /
// was_sketch_valid / hovered_plane) so App.dll hot-reloads don't reset it.

// ──────────────────────────────────────────────────────────────────────────────

static void CanvasRenderToTexture(Scene& scene)
{
    Clay_Dimensions canvas_size = GetDimensions(canvasId);

    if (static_cast<int>(canvas_size.width) != scene.canvas_texture.texture.width
        || static_cast<int>(canvas_size.height) != scene.canvas_texture.texture.height) {
        if (scene.canvas_texture.texture.id != 0)
            UnloadRenderTexture(scene.canvas_texture);
        scene.canvas_texture = LoadRenderTexture(canvas_size.width, canvas_size.height);
    }

    bool is_sketch_active = scene.command_toolbox.IsSketchContext();
    auto active_plane = scene.command_toolbox.GetActiveSketchPlane();
    bool sketch_valid = active_plane.has_value();
    bool pointer_over = Clay_PointerOver(canvasId);

    // Camera snap to the confirmed sketch plane; restore when sketch exits.
    if (sketch_valid && !scene.was_sketch_valid)
        scene.camera.SetOrientation(CanvasCamera::OrientationForSketchPlane(*active_plane));
    if (!is_sketch_active && scene.was_sketch_active)
        scene.camera.SetOrientation(CanvasCamera::CameraOrientation::Isometric_XYZ);

    scene.was_sketch_active = is_sketch_active;
    scene.was_sketch_valid = sketch_valid;

    // Camera input: 2D lock only once the sketch plane is confirmed.
    if (pointer_over) {
        if (sketch_valid)
            scene.camera.ProcessPan2D();
        else
            scene.camera.ProcessPanTilt();
    }

    // ── Plane selection: hover detection + click-to-confirm ──────────────────
    scene.hovered_plane = std::nullopt;
    if (is_sketch_active && !sketch_valid && pointer_over) {
        Ray ray = GetScreenToWorldRay(GetMousePosition(), scene.camera.raylib_camera);
        scene.hovered_plane = ComputeHoveredOriginPlane(ray, ORIGIN_PLANE_EXTENT);

        if (scene.hovered_plane.has_value() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            auto& part_opt = scene.command_toolbox.GetActivePartCommand();
            if (part_opt.has_value()) {
                if (auto* cmd = part_opt.value().As<CreateSketchCommand>())
                    cmd->plane = *scene.hovered_plane;
            }
        }
    }

    // ── Line placement ───────────────────────────────────────────────────────
    if (sketch_valid && pointer_over && scene.command_toolbox.IsSketchCommandActive()) {
        auto& sketch_opt = scene.command_toolbox.GetActiveSketchCommand();
        if (sketch_opt.has_value() && sketch_opt.value().IsType(SketchCommandType::Line)) {
            auto* line_cmd = sketch_opt.value().As<SketchLineCommand>();
            if (line_cmd && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                line_cmd->start = std::nullopt;
            } else if (line_cmd && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                auto hit = scene.camera.GetMouseOnSketchPlane(*active_plane);
                if (!line_cmd->start.has_value()) {
                    line_cmd->start = hit;
                } else {
                    line_cmd->end = hit;
                    Geometry::Point2 chain_start = hit;
                    scene.command_toolbox.FinishSketchCommand();
                    scene.command_toolbox.StartSketchCommand(SketchCommandType::Line);
                    auto& next_opt = scene.command_toolbox.GetActiveSketchCommand();
                    if (next_opt.has_value() && next_opt.value().IsType(SketchCommandType::Line)) {
                        if (auto* next_line = next_opt.value().As<SketchLineCommand>())
                            next_line->start = chain_start;
                    }
                }
            }
        }
    }

    // ── Rendering ────────────────────────────────────────────────────────────
    BeginTextureMode(scene.canvas_texture);
    ClearBackground(WHITE);
    BeginMode3D(scene.camera.raylib_camera);

    if (sketch_valid) {
        // Clean sketch view: grid only, no origin plane quads.
        auto sketch_plane = *active_plane;
        UI::OriginPlane op = SketchPlaneToOriginPlane(sketch_plane);
        UI::DrawGrid(op, 100, 1.0f);

        // Completed sketch lines.
        auto& part_opt = scene.command_toolbox.GetActivePartCommand();
        if (part_opt.has_value()) {
            auto* create_sketch = part_opt.value().As<CreateSketchCommand>();
            if (create_sketch) {
                for (auto& feature : create_sketch->history) {
                    if (!feature.IsType(SketchCommandType::Line))
                        continue;
                    auto* line = feature.As<SketchLineCommand>();
                    if (!line || !line->start.has_value() || !line->end.has_value())
                        continue;
                    DrawLine3D(SketchPointToWorld(*line->start, sketch_plane),
                        SketchPointToWorld(*line->end, sketch_plane),
                        BLACK);
                }

                // Preview: sphere at placed start, preview line, sphere at cursor.
                auto& active_sketch = scene.command_toolbox.GetActiveSketchCommand();
                if (active_sketch.has_value() && active_sketch.value().IsType(SketchCommandType::Line)) {
                    auto* line_cmd = active_sketch.value().As<SketchLineCommand>();
                    if (line_cmd) {
                        auto mouse_pt = scene.camera.GetMouseOnSketchPlane(sketch_plane);
                        Vector3 cursor_world = SketchPointToWorld(mouse_pt, sketch_plane);

                        if (line_cmd->start.has_value()) {
                            Vector3 start_world = SketchPointToWorld(*line_cmd->start, sketch_plane);
                            DrawSphereEx(start_world, 0.1f, 6, 8, BLUE);
                            DrawLine3D(start_world, cursor_world, GRAY);
                        }

                        DrawSphereEx(cursor_world, 0.07f, 6, 8, SKYBLUE);
                    }
                }
            }
        }

    } else if (is_sketch_active) {
        // Plane selection mode: all three origin planes visible and clickable.
        // The hovered plane is highlighted; others are dimmed.
        UI::DrawOriginPlane(UI::OriginPlane::XY, { 0, 0, 0 }, ORIGIN_PLANE_SIZE,
            scene.hovered_plane == Geometry::SketchPlane::XY ? PLANE_COLOR_HOVER : PLANE_COLOR_XY_DIM);
        UI::DrawOriginPlane(UI::OriginPlane::XZ, { 0, 0, 0 }, ORIGIN_PLANE_SIZE,
            scene.hovered_plane == Geometry::SketchPlane::XZ ? PLANE_COLOR_HOVER : PLANE_COLOR_XZ_DIM);
        UI::DrawOriginPlane(UI::OriginPlane::YZ, { 0, 0, 0 }, ORIGIN_PLANE_SIZE,
            scene.hovered_plane == Geometry::SketchPlane::YZ ? PLANE_COLOR_HOVER : PLANE_COLOR_YZ_DIM);
        UI::DrawGrid(UI::OriginPlane::XZ, 100, 1.0f);

    } else {
        // Normal 3D view: grid + completed sketch geometry.
        UI::DrawGrid(UI::OriginPlane::XZ, 100, 1.0f);
        for (auto& sketch : scene.geometry) {
            for (auto& line : sketch.lines) {
                DrawLine3D(SketchPointToWorld(line.start, sketch.plane),
                    SketchPointToWorld(line.end, sketch.plane),
                    BLACK);
            }
        }
    }

    EndMode3D();
    EndTextureMode();
    SetTextureFilter(scene.canvas_texture.texture, TEXTURE_FILTER_ANISOTROPIC_16X);
}

void LayoutCanvas(Scene& scene)
{
    static constexpr float CANVAS_WIDTH_SHRINK_MIN { 500.0f };

    CLAY({ .id = canvasId,
        .layout = {
            .sizing = LAYOUT_EXPAND_MIN_MAX_WIDTH(CANVAS_WIDTH_SHRINK_MIN),
        },
        .custom = ClayCustom_TextureRenderConfig(scene.canvas_texture) })
    {
        CanvasRenderToTexture(scene);
    };
}
