#include "Camera.h"
#include <algorithm>

namespace Viewport
{

using namespace DcadMath;

void Camera::resetCamera()
{
    projection_ = Projection::Perspective;
    fov_y_ = 45.0f;
    setOrientation(Orientation::Isometric);
}

void Camera::setOrientation(Orientation orientation)
{
    // preserve current zoom distance across the orientation snap
    f32 distance = getDistanceToTarget();
    if(distance < MIN_DISTANCE)
    {
        distance = 25.0f;
    }
    target_ = { 0.0f, 0.0f, 0.0f };

    switch(orientation)
    {
    case Orientation::Isometric:
    {
        Vector3 direction = Normalize(Vector3 { 1.0f, 1.0f, 1.0f });
        position_ = direction * distance;
        up_ = { 0.0f, 1.0f, 0.0f };
        break;
    }
    case Orientation::PlaneXY:
        position_ = { 0.0f, 0.0f, distance };
        up_ = { 0.0f, 1.0f, 0.0f };
        break;
    case Orientation::PlaneXZ:
        position_ = { 0.0f, distance, 0.0f };
        up_ = { 0.0f, 0.0f, -1.0f };
        break;
    case Orientation::PlaneYZ:
        position_ = { distance, 0.0f, 0.0f };
        up_ = { 0.0f, 1.0f, 0.0f };
        break;
    }
}

void Camera::orbitAroundTarget(f32 delta_x, f32 delta_y)
{
    if(delta_x == 0.0f && delta_y == 0.0f)
    {
        return;
    }

    Vector3 offset = position_ - target_;

    // yaw about world up keeps the horizon level regardless of orbit angle
    f32 yaw = -delta_x * ORBIT_SENSITIVITY;
    Vector3 world_up { 0.0f, 1.0f, 0.0f };
    offset = RotateAxisAngle(offset, world_up, yaw);
    up_ = Normalize(RotateAxisAngle(up_, world_up, yaw));

    f32 pitch = -delta_y * ORBIT_SENSITIVITY;
    Vector3 forward = Normalize(-offset);
    Vector3 right = Cross(forward, up_);

    // near the poles, forward and up are nearly parallel and the cross
    // product collapses; skip the pitch rather than snapping to a random axis
    if(LengthSquared(right) > 1e-8f)
    {
        right = Normalize(right);
        Vector3 rotated = RotateAxisAngle(offset, right, pitch);
        Vector3 new_up = Normalize(RotateAxisAngle(up_, right, pitch));
        offset = rotated;
        up_ = new_up;
    }

    position_ = target_ + offset;
}

void Camera::panAcrossView(f32 delta_x, f32 delta_y)
{
    if(delta_x == 0.0f && delta_y == 0.0f)
    {
        return;
    }

    // panning scales with distance: keeps the world tracking the cursor at any zoom level
    f32 scale = PAN_SENSITIVITY * std::max(getDistanceToTarget(), MIN_DISTANCE);

    Vector3 forward = Normalize(target_ - position_);
    Vector3 right = Normalize(Cross(forward, up_));
    Vector3 up = Cross(right, forward);

    Vector3 movement = right * (-delta_x * scale) + up * (delta_y * scale);
    position_ += movement;
    target_ += movement;
}

void Camera::zoomTowardTarget(f32 amount)
{
    if(amount == 0.0f)
    {
        return;
    }

    Vector3 offset = position_ - target_;
    f32 distance = Length(offset);
    if(distance < 1e-6f)
    {
        return;
    }

    // exponential curve keeps each notch feeling the same at every scale
    f32 new_distance = distance * std::exp(-amount * ZOOM_SENSITIVITY);
    new_distance = std::clamp(new_distance, MIN_DISTANCE, MAX_DISTANCE);

    position_ = target_ + Normalize(offset) * new_distance;
}

Matrix4 Camera::getViewMatrix() const
{
    return MatrixLookAt(position_, target_, up_);
}

Matrix4 Camera::getProjectionMatrix(f32 aspect) const
{
    if(aspect <= 0.0f)
    {
        aspect = 1.0f;
    }
    if(projection_ == Projection::Perspective)
    {
        return MatrixPerspective(fov_y_ * DEGREES_TO_RADIANS, aspect, near_, far_);
    }
    // match the perspective view's framing at the current distance
    f32 half_height = getDistanceToTarget() * std::tan(fov_y_ * DEGREES_TO_RADIANS * 0.5f);
    f32 half_width = half_height * aspect;
    return MatrixOrthographic(-half_width, half_width, -half_height, half_height, near_, far_);
}

Ray Camera::screenPointToRay(f32 x, f32 y, f32 viewport_width, f32 viewport_height) const
{
    Ray ray {};
    if(viewport_width <= 0.0f || viewport_height <= 0.0f)
    {
        return ray;
    }

    // pixel -> NDC; Y flips since pixel space grows downward while NDC grows upward
    f32 ndc_x = (2.0f * x) / viewport_width - 1.0f;
    f32 ndc_y = 1.0f - (2.0f * y) / viewport_height;

    f32 aspect = viewport_width / viewport_height;
    Matrix4 inverse_view_projection = Inverse(getProjectionMatrix(aspect) * getViewMatrix());

    // WebGPU depth range is [0, 1]: 0 is the near plane, 1 is the far plane
    Vector4 near_homogeneous = inverse_view_projection * Vector4 { ndc_x, ndc_y, 0.0f, 1.0f };
    Vector4 far_homogeneous = inverse_view_projection * Vector4 { ndc_x, ndc_y, 1.0f, 1.0f };

    if(near_homogeneous.w == 0.0f || far_homogeneous.w == 0.0f)
    {
        return ray;
    }

    Vector3 near_point { near_homogeneous.x / near_homogeneous.w, near_homogeneous.y / near_homogeneous.w, near_homogeneous.z / near_homogeneous.w };
    Vector3 far_point { far_homogeneous.x / far_homogeneous.w, far_homogeneous.y / far_homogeneous.w, far_homogeneous.z / far_homogeneous.w };

    ray.origin = near_point;
    ray.direction = Normalize(far_point - near_point);
    return ray;
}

} // namespace Viewport
