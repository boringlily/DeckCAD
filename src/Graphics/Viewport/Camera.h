#pragma once
#include "DeckMath.h"
#include "Types.h"

namespace Viewport {

/// Orbit camera for the CAD viewport.
///
/// Takes already-resolved input deltas rather than polling a device, so the
/// camera can be unit tested and is not tied to any windowing library.
class Camera {
public:
    enum class Orientation {
        Isometric,
        PlaneXY,
        PlaneXZ,
        PlaneYZ,
    };

    enum class Projection {
        Perspective,
        Orthographic,
    };

    Camera() { resetCamera(); }

    void resetCamera();
    void setOrientation(Orientation orientation);

    /// Orbit around the target. Deltas are in pixels.
    void orbitAroundTarget(f32 delta_x, f32 delta_y);

    /// Pan the camera and its target across the view plane. Deltas are in pixels.
    void panAcrossView(f32 delta_x, f32 delta_y);

    /// Dolly toward/away from the target. Positive @p amount moves closer.
    /// Distance is clamped so the camera can neither cross nor escape the target.
    void zoomTowardTarget(f32 amount);

    DeckMath::Matrix4 getViewMatrix() const;
    DeckMath::Matrix4 getProjectionMatrix(f32 aspect) const;

    /// Build a world-space ray through a viewport pixel.
    /// @p x, @p y are in pixels with the origin at the viewport's top-left.
    DeckMath::Ray screenPointToRay(f32 x, f32 y, f32 viewport_width, f32 viewport_height) const;

    DeckMath::Vector3 getPosition() const { return position_; }
    DeckMath::Vector3 getTarget() const { return target_; }
    DeckMath::Vector3 getUp() const { return up_; }
    f32 getDistanceToTarget() const { return DeckMath::Distance(position_, target_); }

    Projection getProjection() const { return projection_; }
    void setProjection(Projection p) { projection_ = p; }

    f32 getFovYDegrees() const { return fov_y_; }
    f32 getNearPlane() const { return near_; }
    f32 getFarPlane() const { return far_; }

    // Tuning constants, kept public so the UI can expose them later.
    static constexpr f32 ORBIT_SENSITIVITY = 0.005f;
    static constexpr f32 PAN_SENSITIVITY = 0.0015f;
    static constexpr f32 ZOOM_SENSITIVITY = 0.12f;
    static constexpr f32 MIN_DISTANCE = 0.05f;
    static constexpr f32 MAX_DISTANCE = 5000.0f;

private:
    DeckMath::Vector3 position_ { 15.0f, 15.0f, 15.0f };
    DeckMath::Vector3 target_ { 0.0f, 0.0f, 0.0f };
    DeckMath::Vector3 up_ { 0.0f, 1.0f, 0.0f };

    Projection projection_ { Projection::Perspective };
    f32 fov_y_ { 45.0f };
    f32 near_ { 0.05f };
    f32 far_ { 2000.0f };
};

} // namespace Viewport
