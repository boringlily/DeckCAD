#include "DeckMath.h"
#include <gtest/gtest.h>

using namespace DeckMath;

namespace MathTestsInternal {

constexpr f32 EPSILON = 1e-4f;

void ExpectVector3Near(Vector3 actual, Vector3 expected, f32 tolerance = EPSILON)
{
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
}

/// Projects a view-space point and returns its NDC depth.
f32 ProjectedDepth(const Matrix4& projection_ref, Vector3 view_space_point)
{
    Vector4 clip = projection_ref * Vector4 { view_space_point.x, view_space_point.y, view_space_point.z, 1.0f };
    return clip.z / clip.w;
}

} // namespace MathTestsInternal
using namespace MathTestsInternal;

TEST(Matrix4, IdentityIsMultiplicativeIdentity)
{
    Matrix4 identity = Matrix4::identityMatrix();
    Matrix4 transform = MatrixTranslate({ 3.0f, -2.0f, 7.5f }) * MatrixScale({ 2.0f, 2.0f, 2.0f });

    Matrix4 result = transform * identity;
    for (int i = 0; i < 16; ++i) {
        EXPECT_NEAR((&result.columns[0].x)[i], (&transform.columns[0].x)[i], EPSILON);
    }
}

TEST(Matrix4, TranslationLandsInFourthColumn)
{
    Matrix4 translate = MatrixTranslate({ 1.0f, 2.0f, 3.0f });
    Vector4 moved = translate * Vector4 { 0.0f, 0.0f, 0.0f, 1.0f };

    EXPECT_NEAR(moved.x, 1.0f, EPSILON);
    EXPECT_NEAR(moved.y, 2.0f, EPSILON);
    EXPECT_NEAR(moved.z, 3.0f, EPSILON);
    EXPECT_NEAR(moved.w, 1.0f, EPSILON);
}

TEST(Matrix4, InverseRoundTripsAGeneralTransform)
{
    Matrix4 transform = MatrixTranslate({ 4.0f, -1.0f, 2.0f })
        * MatrixLookAt({ 3.0f, 4.0f, 5.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f })
        * MatrixScale({ 2.0f, 0.5f, 3.0f });

    Matrix4 round_trip = transform * Inverse(transform);
    Matrix4 identity = Matrix4::identityMatrix();

    for (int i = 0; i < 16; ++i) {
        EXPECT_NEAR((&round_trip.columns[0].x)[i], (&identity.columns[0].x)[i], 1e-3f);
    }
}

TEST(Matrix4, SingularMatrixInverseFallsBackToIdentity)
{
    // A zero scale collapses the matrix; the fallback keeps NaNs out of the
    // camera path rather than propagating them into every ray.
    Matrix4 singular = MatrixScale({ 1.0f, 0.0f, 1.0f });
    Matrix4 result = Inverse(singular);
    Matrix4 identity = Matrix4::identityMatrix();

    for (int i = 0; i < 16; ++i) {
        EXPECT_NEAR((&result.columns[0].x)[i], (&identity.columns[0].x)[i], EPSILON);
    }
}

// The whole renderer assumes WebGPU's [0, 1] clip depth. Under the OpenGL
// convention raylib used, the near plane would map to -1 and everything would
// depth-test wrong, so this is the guard against silently regressing back.
TEST(Projection, PerspectiveMapsNearToZeroAndFarToOne)
{
    const f32 near_z = 0.1f;
    const f32 far_z = 100.0f;
    Matrix4 projection = MatrixPerspective(60.0f * DEGREES_TO_RADIANS, 16.0f / 9.0f, near_z, far_z);

    EXPECT_NEAR(ProjectedDepth(projection, { 0.0f, 0.0f, -near_z }), 0.0f, EPSILON);
    EXPECT_NEAR(ProjectedDepth(projection, { 0.0f, 0.0f, -far_z }), 1.0f, EPSILON);
}

TEST(Projection, PerspectiveDepthIncreasesWithDistance)
{
    Matrix4 projection = MatrixPerspective(60.0f * DEGREES_TO_RADIANS, 1.0f, 0.1f, 100.0f);

    f32 close = ProjectedDepth(projection, { 0.0f, 0.0f, -1.0f });
    f32 middle = ProjectedDepth(projection, { 0.0f, 0.0f, -10.0f });
    f32 distant = ProjectedDepth(projection, { 0.0f, 0.0f, -50.0f });

    EXPECT_LT(close, middle);
    EXPECT_LT(middle, distant);
}

TEST(Projection, OrthographicMapsNearToZeroAndFarToOne)
{
    const f32 near_z = 0.5f;
    const f32 far_z = 200.0f;
    Matrix4 projection = MatrixOrthographic(-10.0f, 10.0f, -5.0f, 5.0f, near_z, far_z);

    EXPECT_NEAR(ProjectedDepth(projection, { 0.0f, 0.0f, -near_z }), 0.0f, EPSILON);
    EXPECT_NEAR(ProjectedDepth(projection, { 0.0f, 0.0f, -far_z }), 1.0f, EPSILON);
}

TEST(LookAt, PlacesTheEyeAtTheViewSpaceOrigin)
{
    Vector3 eye { 5.0f, 5.0f, 5.0f };
    Matrix4 view = MatrixLookAt(eye, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });

    Vector4 eye_in_view = view * Vector4 { eye.x, eye.y, eye.z, 1.0f };
    EXPECT_NEAR(eye_in_view.x, 0.0f, EPSILON);
    EXPECT_NEAR(eye_in_view.y, 0.0f, EPSILON);
    EXPECT_NEAR(eye_in_view.z, 0.0f, EPSILON);
}

TEST(LookAt, PutsTheTargetOnTheNegativeZAxis)
{
    Matrix4 view = MatrixLookAt({ 0.0f, 0.0f, 10.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
    Vector4 target = view * Vector4 { 0.0f, 0.0f, 0.0f, 1.0f };

    EXPECT_NEAR(target.x, 0.0f, EPSILON);
    EXPECT_NEAR(target.y, 0.0f, EPSILON);
    EXPECT_NEAR(target.z, -10.0f, EPSILON); // camera looks down its own -Z
}

TEST(Vector, CrossProductFollowsTheRightHandRule)
{
    ExpectVector3Near(Cross({ 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }), { 0.0f, 0.0f, 1.0f });
}

TEST(Vector, NormalizeOfZeroVectorStaysZero)
{
    // Guards the divide-by-zero path; a NaN here would poison a whole matrix.
    ExpectVector3Near(Normalize({ 0.0f, 0.0f, 0.0f }), { 0.0f, 0.0f, 0.0f });
}

TEST(Vector, RotateAxisAngleQuarterTurnAboutY)
{
    Vector3 rotated = RotateAxisAngle({ 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, PI * 0.5f);
    ExpectVector3Near(rotated, { 0.0f, 0.0f, -1.0f });
}

TEST(RayPlane, HitsTheGroundPlaneInFront)
{
    Ray ray { { 0.0f, 5.0f, 0.0f }, Normalize({ 0.0f, -1.0f, 0.0f }) };
    Vector3 hit {};

    ASSERT_TRUE(RayPlaneIntersect(ray, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, hit));
    ExpectVector3Near(hit, { 0.0f, 0.0f, 0.0f });
}

TEST(RayPlane, MissesWhenParallel)
{
    Ray ray { { 0.0f, 5.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } };
    Vector3 hit {};
    EXPECT_FALSE(RayPlaneIntersect(ray, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, hit));
}

TEST(RayPlane, MissesWhenThePlaneIsBehindTheRay)
{
    Ray ray { { 0.0f, 5.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } }; // pointing away
    Vector3 hit {};
    EXPECT_FALSE(RayPlaneIntersect(ray, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, hit));
}
