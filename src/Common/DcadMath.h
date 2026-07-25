#pragma once
#include "Types.h"
#include <cmath>

/**
 * @brief Minimal linear algebra for the DeckCAD viewport.
 * @note Conventions are chosen to match WebGPU, not OpenGL:
 * - Right-handed world space; the camera looks down its local -Z.
 * - Clip space depth maps to [0, 1]. OpenGL/raylib used [-1, 1]; every
 *   projection here maps near->0 and far->1 instead.
 * - Matrices are column-major and stored as four columns, matching the
 *   layout WGSL's mat4x4<f32> expects; they upload without transposing.
 */

namespace DcadMath
{

inline constexpr f32 PI = 3.14159265358979323846f;
inline constexpr f32 DEGREES_TO_RADIANS = PI / 180.0f;
inline constexpr f32 RADIANS_TO_DEGREES = 180.0f / PI;

struct Vector2
{
    f32 x { 0 }, y { 0 };
};

struct Vector3
{
    f32 x { 0 }, y { 0 }, z { 0 };
};

struct Vector4
{
    f32 x { 0 }, y { 0 }, z { 0 }, w { 0 };
};

// --- Vector2 ----------------------------------------------------------------
inline Vector2 operator+(Vector2 left, Vector2 right) { return { left.x + right.x, left.y + right.y }; }
inline Vector2 operator-(Vector2 left, Vector2 right) { return { left.x - right.x, left.y - right.y }; }
inline Vector2 operator*(Vector2 vector, f32 scalar) { return { vector.x * scalar, vector.y * scalar }; }

// --- Vector3 ----------------------------------------------------------------
inline Vector3 operator+(Vector3 left, Vector3 right) { return { left.x + right.x, left.y + right.y, left.z + right.z }; }
inline Vector3 operator-(Vector3 left, Vector3 right) { return { left.x - right.x, left.y - right.y, left.z - right.z }; }
inline Vector3 operator-(Vector3 vector) { return { -vector.x, -vector.y, -vector.z }; }
inline Vector3 operator*(Vector3 vector, f32 scalar) { return { vector.x * scalar, vector.y * scalar, vector.z * scalar }; }
inline Vector3 operator*(f32 scalar, Vector3 vector) { return vector * scalar; }
inline Vector3 operator/(Vector3 vector, f32 scalar) { return { vector.x / scalar, vector.y / scalar, vector.z / scalar }; }
inline Vector3& operator+=(Vector3& left_ref, Vector3 right)
{
    left_ref = left_ref + right;
    return left_ref;
}
inline Vector3& operator-=(Vector3& left_ref, Vector3 right)
{
    left_ref = left_ref - right;
    return left_ref;
}

inline f32 Dot(Vector3 left, Vector3 right) { return left.x * right.x + left.y * right.y + left.z * right.z; }

inline Vector3 Cross(Vector3 left, Vector3 right)
{
    return { left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x };
}

inline f32 LengthSquared(Vector3 vector) { return Dot(vector, vector); }
inline f32 Length(Vector3 vector) { return std::sqrt(Dot(vector, vector)); }
inline f32 Distance(Vector3 from, Vector3 to) { return Length(to - from); }

inline Vector3 Normalize(Vector3 vector)
{
    f32 length = Length(vector);
    // degenerate input divides by zero, return zero vector not NaN
    return length > 0.0f ? vector / length : Vector3 {};
}

inline Vector3 Lerp(Vector3 from, Vector3 to, f32 amount) { return from + (to - from) * amount; }

/// Rotate @p vector around @p axis (must be normalized) by @p radians. Rodrigues' formula.
inline Vector3 RotateAxisAngle(Vector3 vector, Vector3 axis, f32 radians)
{
    f32 cosine = std::cos(radians);
    f32 sine = std::sin(radians);
    return vector * cosine + Cross(axis, vector) * sine + axis * (Dot(axis, vector) * (1.0f - cosine));
}

// --- Matrix4 ----------------------------------------------------------------

/// Column-major 4x4 matrix; `columns[i]` is the i-th *column* and `columns[3]`
/// holds the translation of an affine transform.
struct Matrix4
{
    Vector4 columns[4] {};

    static Matrix4 identityMatrix()
    {
        Matrix4 result;
        result.columns[0] = { 1, 0, 0, 0 };
        result.columns[1] = { 0, 1, 0, 0 };
        result.columns[2] = { 0, 0, 1, 0 };
        result.columns[3] = { 0, 0, 0, 1 };
        return result;
    }
};

inline Matrix4 operator*(const Matrix4& left_ref, const Matrix4& right_ref)
{
    Matrix4 result;
    for(int column = 0; column < 4; ++column)
    {
        const Vector4& right_column_ref = right_ref.columns[column];
        result.columns[column] = {
            left_ref.columns[0].x * right_column_ref.x + left_ref.columns[1].x * right_column_ref.y + left_ref.columns[2].x * right_column_ref.z + left_ref.columns[3].x * right_column_ref.w,
            left_ref.columns[0].y * right_column_ref.x + left_ref.columns[1].y * right_column_ref.y + left_ref.columns[2].y * right_column_ref.z + left_ref.columns[3].y * right_column_ref.w,
            left_ref.columns[0].z * right_column_ref.x + left_ref.columns[1].z * right_column_ref.y + left_ref.columns[2].z * right_column_ref.z + left_ref.columns[3].z * right_column_ref.w,
            left_ref.columns[0].w * right_column_ref.x + left_ref.columns[1].w * right_column_ref.y + left_ref.columns[2].w * right_column_ref.z + left_ref.columns[3].w * right_column_ref.w,
        };
    }
    return result;
}

inline Vector4 operator*(const Matrix4& matrix_ref, Vector4 vector)
{
    return {
        matrix_ref.columns[0].x * vector.x + matrix_ref.columns[1].x * vector.y + matrix_ref.columns[2].x * vector.z + matrix_ref.columns[3].x * vector.w,
        matrix_ref.columns[0].y * vector.x + matrix_ref.columns[1].y * vector.y + matrix_ref.columns[2].y * vector.z + matrix_ref.columns[3].y * vector.w,
        matrix_ref.columns[0].z * vector.x + matrix_ref.columns[1].z * vector.y + matrix_ref.columns[2].z * vector.z + matrix_ref.columns[3].z * vector.w,
        matrix_ref.columns[0].w * vector.x + matrix_ref.columns[1].w * vector.y + matrix_ref.columns[2].w * vector.z + matrix_ref.columns[3].w * vector.w,
    };
}

inline Matrix4 MatrixTranslate(Vector3 translation)
{
    Matrix4 result = Matrix4::identityMatrix();
    result.columns[3] = { translation.x, translation.y, translation.z, 1 };
    return result;
}

inline Matrix4 MatrixScale(Vector3 scale)
{
    Matrix4 result = Matrix4::identityMatrix();
    result.columns[0].x = scale.x;
    result.columns[1].y = scale.y;
    result.columns[2].z = scale.z;
    return result;
}

/// Right-handed look-at. Camera sits at @p eye looking toward @p target.
inline Matrix4 MatrixLookAt(Vector3 eye, Vector3 target, Vector3 up)
{
    Vector3 forward = Normalize(target - eye);
    Vector3 side = Normalize(Cross(forward, up));
    Vector3 true_up = Cross(side, forward);

    Matrix4 result = Matrix4::identityMatrix();
    result.columns[0] = { side.x, true_up.x, -forward.x, 0 };
    result.columns[1] = { side.y, true_up.y, -forward.y, 0 };
    result.columns[2] = { side.z, true_up.z, -forward.z, 0 };
    result.columns[3] = { -Dot(side, eye), -Dot(true_up, eye), Dot(forward, eye), 1 };
    return result;
}

/// Right-handed perspective projection with a [0, 1] depth range (WebGPU).
inline Matrix4 MatrixPerspective(f32 fov_y_radians, f32 aspect, f32 near_z, f32 far_z)
{
    f32 focal_length = 1.0f / std::tan(fov_y_radians * 0.5f);
    Matrix4 result {}; // deliberately zero-initialized, not an affine matrix
    result.columns[0].x = focal_length / aspect;
    result.columns[1].y = focal_length;
    result.columns[2].z = far_z / (near_z - far_z);
    result.columns[2].w = -1.0f;
    result.columns[3].z = (far_z * near_z) / (near_z - far_z);
    return result;
}

/// Right-handed orthographic projection with a [0, 1] depth range (WebGPU).
inline Matrix4 MatrixOrthographic(f32 left, f32 right, f32 bottom, f32 top, f32 near_z, f32 far_z)
{
    Matrix4 result = Matrix4::identityMatrix();
    result.columns[0].x = 2.0f / (right - left);
    result.columns[1].y = 2.0f / (top - bottom);
    result.columns[2].z = 1.0f / (near_z - far_z);
    result.columns[3].x = -(right + left) / (right - left);
    result.columns[3].y = -(top + bottom) / (top - bottom);
    result.columns[3].z = near_z / (near_z - far_z);
    return result;
}

/// General 4x4 inverse via cofactor expansion. Returns identity for a singular
/// matrix: a bad camera state degrades visibly instead of producing NaNs.
inline Matrix4 Inverse(const Matrix4& matrix_ref)
{
    const f32* source_ptr = &matrix_ref.columns[0].x; // column-major, source_ptr[column * 4 + row]

    f32 minor_0 = source_ptr[0] * source_ptr[5] - source_ptr[4] * source_ptr[1];
    f32 minor_1 = source_ptr[0] * source_ptr[9] - source_ptr[8] * source_ptr[1];
    f32 minor_2 = source_ptr[0] * source_ptr[13] - source_ptr[12] * source_ptr[1];
    f32 minor_3 = source_ptr[4] * source_ptr[9] - source_ptr[8] * source_ptr[5];
    f32 minor_4 = source_ptr[4] * source_ptr[13] - source_ptr[12] * source_ptr[5];
    f32 minor_5 = source_ptr[8] * source_ptr[13] - source_ptr[12] * source_ptr[9];

    f32 cofactor_5 = source_ptr[10] * source_ptr[15] - source_ptr[14] * source_ptr[11];
    f32 cofactor_4 = source_ptr[6] * source_ptr[15] - source_ptr[14] * source_ptr[7];
    f32 cofactor_3 = source_ptr[6] * source_ptr[11] - source_ptr[10] * source_ptr[7];
    f32 cofactor_2 = source_ptr[2] * source_ptr[15] - source_ptr[14] * source_ptr[3];
    f32 cofactor_1 = source_ptr[2] * source_ptr[11] - source_ptr[10] * source_ptr[3];
    f32 cofactor_0 = source_ptr[2] * source_ptr[7] - source_ptr[6] * source_ptr[3];

    f32 determinant = minor_0 * cofactor_5 - minor_1 * cofactor_4 + minor_2 * cofactor_3
        + minor_3 * cofactor_2 - minor_4 * cofactor_1 + minor_5 * cofactor_0;
    if(std::fabs(determinant) < 1e-20f)
    {
        return Matrix4::identityMatrix();
    }
    f32 inverse_determinant = 1.0f / determinant;

    Matrix4 result;
    f32* output_ptr = &result.columns[0].x;
    output_ptr[0] = (source_ptr[5] * cofactor_5 - source_ptr[9] * cofactor_4 + source_ptr[13] * cofactor_3) * inverse_determinant;
    output_ptr[1] = (-source_ptr[1] * cofactor_5 + source_ptr[9] * cofactor_2 - source_ptr[13] * cofactor_1) * inverse_determinant;
    output_ptr[2] = (source_ptr[1] * cofactor_4 - source_ptr[5] * cofactor_2 + source_ptr[13] * cofactor_0) * inverse_determinant;
    output_ptr[3] = (-source_ptr[1] * cofactor_3 + source_ptr[5] * cofactor_1 - source_ptr[9] * cofactor_0) * inverse_determinant;

    output_ptr[4] = (-source_ptr[4] * cofactor_5 + source_ptr[8] * cofactor_4 - source_ptr[12] * cofactor_3) * inverse_determinant;
    output_ptr[5] = (source_ptr[0] * cofactor_5 - source_ptr[8] * cofactor_2 + source_ptr[12] * cofactor_1) * inverse_determinant;
    output_ptr[6] = (-source_ptr[0] * cofactor_4 + source_ptr[4] * cofactor_2 - source_ptr[12] * cofactor_0) * inverse_determinant;
    output_ptr[7] = (source_ptr[0] * cofactor_3 - source_ptr[4] * cofactor_1 + source_ptr[8] * cofactor_0) * inverse_determinant;

    output_ptr[8] = (source_ptr[7] * minor_5 - source_ptr[11] * minor_4 + source_ptr[15] * minor_3) * inverse_determinant;
    output_ptr[9] = (-source_ptr[3] * minor_5 + source_ptr[11] * minor_2 - source_ptr[15] * minor_1) * inverse_determinant;
    output_ptr[10] = (source_ptr[3] * minor_4 - source_ptr[7] * minor_2 + source_ptr[15] * minor_0) * inverse_determinant;
    output_ptr[11] = (-source_ptr[3] * minor_3 + source_ptr[7] * minor_1 - source_ptr[11] * minor_0) * inverse_determinant;

    output_ptr[12] = (-source_ptr[6] * minor_5 + source_ptr[10] * minor_4 - source_ptr[14] * minor_3) * inverse_determinant;
    output_ptr[13] = (source_ptr[2] * minor_5 - source_ptr[10] * minor_2 + source_ptr[14] * minor_1) * inverse_determinant;
    output_ptr[14] = (-source_ptr[2] * minor_4 + source_ptr[6] * minor_2 - source_ptr[14] * minor_0) * inverse_determinant;
    output_ptr[15] = (source_ptr[2] * minor_3 - source_ptr[6] * minor_1 + source_ptr[10] * minor_0) * inverse_determinant;
    return result;
}

// --- Ray --------------------------------------------------------------------

struct Ray
{
    Vector3 origin {};
    Vector3 direction {};
};

/// Intersect a ray with the plane through @p plane_point with normal @p plane_normal.
/// Returns false when the ray is parallel to the plane or the hit is behind it.
inline bool RayPlaneIntersect(const Ray& ray_ref, Vector3 plane_point, Vector3 plane_normal, Vector3& out_hit_ref)
{
    f32 denominator = Dot(plane_normal, ray_ref.direction);
    if(std::fabs(denominator) < 1e-6f)
    {
        return false;
    }
    f32 distance_along_ray = Dot(plane_point - ray_ref.origin, plane_normal) / denominator;
    if(distance_along_ray < 0.0f)
    {
        return false;
    }
    out_hit_ref = ray_ref.origin + ray_ref.direction * distance_along_ray;
    return true;
}

} // namespace DcadMath
