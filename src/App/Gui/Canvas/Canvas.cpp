#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include "rcamera.h"

#include <optional>

#include "Geometry.h"
#include "SketchFeature.h"
#include "CreateSketchCommand.h"

#include "CanvasPlanes.cpp"

// Shared 3D canvas helpers reused by the Ui SceneViewport / UiCanvas (App.cpp).
// The Clay rendering + layout (CanvasRenderToTexture / LayoutCanvas) was removed
// with the Clay teardown; the Ui path renders via Ui::Raylib::Canvas3D.

// Half-extent of the rendered (and hittable) origin plane quads.
static constexpr float ORIGIN_PLANE_EXTENT { 5.0f };
static constexpr Vector2 ORIGIN_PLANE_SIZE { 10.0f, 10.0f };

// Highlight when hovering a plane in selection mode; dimmed tint for the rest.
static constexpr Color PLANE_COLOR_HOVER { 255, 200, 50, 160 };
static constexpr Color PLANE_COLOR_XY_DIM { 50, 100, 230, 30 };
static constexpr Color PLANE_COLOR_XZ_DIM { 50, 200, 80, 30 };
static constexpr Color PLANE_COLOR_YZ_DIM { 230, 60, 60, 30 };

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
