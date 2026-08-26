#include "Camera.h"
#include <gtest/gtest.h>

using Viewport::Camera;
using namespace DeckMath;

namespace CameraTestsInternal {
constexpr f32 EPSILON = 1e-3f;
}
using namespace CameraTestsInternal;

TEST(Camera, StartsIsometricLookingAtTheOrigin)
{
    Camera camera;
    EXPECT_NEAR(camera.getTarget().x, 0.0f, EPSILON);
    EXPECT_NEAR(camera.getTarget().y, 0.0f, EPSILON);
    EXPECT_NEAR(camera.getTarget().z, 0.0f, EPSILON);

    // Equal components on all three axes is what makes the view isometric.
    Vector3 position = camera.getPosition();
    EXPECT_NEAR(position.x, position.y, EPSILON);
    EXPECT_NEAR(position.y, position.z, EPSILON);
    EXPECT_GT(position.x, 0.0f);
}

TEST(Camera, OrbitPreservesDistanceToTarget)
{
    Camera camera;
    const f32 before = camera.getDistanceToTarget();

    camera.orbitAroundTarget(120.0f, 45.0f);

    EXPECT_NEAR(camera.getDistanceToTarget(), before, EPSILON);
}

TEST(Camera, OrbitActuallyMovesTheCamera)
{
    Camera camera;
    Vector3 before = camera.getPosition();

    camera.orbitAroundTarget(150.0f, 0.0f);

    EXPECT_GT(Distance(camera.getPosition(), before), 0.01f);
}

TEST(Camera, OrbitWithNoInputIsANoOp)
{
    Camera camera;
    Vector3 before = camera.getPosition();

    camera.orbitAroundTarget(0.0f, 0.0f);

    EXPECT_NEAR(Distance(camera.getPosition(), before), 0.0f, EPSILON);
}

TEST(Camera, PanMovesPositionAndTargetTogether)
{
    Camera camera;
    const Vector3 offset_before = camera.getPosition() - camera.getTarget();

    camera.panAcrossView(50.0f, 25.0f);

    const Vector3 offset_after = camera.getPosition() - camera.getTarget();
    // Panning slides the whole rig, so the eye-to-target vector is unchanged.
    EXPECT_NEAR(offset_after.x, offset_before.x, EPSILON);
    EXPECT_NEAR(offset_after.y, offset_before.y, EPSILON);
    EXPECT_NEAR(offset_after.z, offset_before.z, EPSILON);
    EXPECT_GT(Distance(camera.getTarget(), { 0.0f, 0.0f, 0.0f }), 0.01f);
}

TEST(Camera, ZoomInMovesCloserAndZoomOutMovesAway)
{
    Camera camera;
    const f32 start = camera.getDistanceToTarget();

    camera.zoomTowardTarget(1.0f);
    const f32 closer = camera.getDistanceToTarget();
    EXPECT_LT(closer, start);

    camera.zoomTowardTarget(-1.0f);
    // Exponential zoom is symmetric, so this returns to where it began.
    EXPECT_NEAR(camera.getDistanceToTarget(), start, EPSILON);
}

TEST(Camera, ZoomIsClampedAtBothEnds)
{
    Camera camera;

    for (int i = 0; i < 500; ++i) {
        camera.zoomTowardTarget(1.0f);
    }
    EXPECT_GE(camera.getDistanceToTarget(), Camera::MIN_DISTANCE - EPSILON);

    for (int i = 0; i < 1000; ++i) {
        camera.zoomTowardTarget(-1.0f);
    }
    EXPECT_LE(camera.getDistanceToTarget(), Camera::MAX_DISTANCE + EPSILON);
}

TEST(Camera, SetOrientationKeepsTheCurrentZoomLevel)
{
    Camera camera;
    camera.zoomTowardTarget(2.0f);
    const f32 distance = camera.getDistanceToTarget();

    camera.setOrientation(Camera::Orientation::PlaneXY);

    EXPECT_NEAR(camera.getDistanceToTarget(), distance, EPSILON);
    // Looking down -Z at the XY plane puts the eye on the +Z axis.
    EXPECT_NEAR(camera.getPosition().x, 0.0f, EPSILON);
    EXPECT_NEAR(camera.getPosition().y, 0.0f, EPSILON);
    EXPECT_GT(camera.getPosition().z, 0.0f);
}

TEST(Camera, CentreRayPointsFromTheEyeTowardTheTarget)
{
    Camera camera;
    Ray ray = camera.screenPointToRay(400.0f, 300.0f, 800.0f, 600.0f);

    const Vector3 expected = Normalize(camera.getTarget() - camera.getPosition());
    EXPECT_NEAR(ray.direction.x, expected.x, 1e-2f);
    EXPECT_NEAR(ray.direction.y, expected.y, 1e-2f);
    EXPECT_NEAR(ray.direction.z, expected.z, 1e-2f);
}

TEST(Camera, CentreRayHitsTheGroundPlaneAtTheTarget)
{
    Camera camera;
    camera.setOrientation(Camera::Orientation::PlaneXZ); // straight down at Y=0

    Ray ray = camera.screenPointToRay(400.0f, 300.0f, 800.0f, 600.0f);
    Vector3 hit {};
    ASSERT_TRUE(RayPlaneIntersect(ray, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, hit));

    EXPECT_NEAR(hit.x, 0.0f, 1e-2f);
    EXPECT_NEAR(hit.z, 0.0f, 1e-2f);
}

TEST(Camera, EdgeRaysDivergeFromTheCentreRay)
{
    Camera camera;
    Ray centre = camera.screenPointToRay(400.0f, 300.0f, 800.0f, 600.0f);
    Ray corner = camera.screenPointToRay(0.0f, 0.0f, 800.0f, 600.0f);

    EXPECT_LT(Dot(centre.direction, corner.direction), 0.999f);
}

TEST(Camera, ScreenPointToRayHandlesADegenerateViewport)
{
    Camera camera;
    // A zero-sized viewport happens for a frame while a panel is being docked.
    Ray ray = camera.screenPointToRay(0.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_NEAR(Length(ray.direction), 0.0f, EPSILON);
}

TEST(Camera, OrthographicProjectionDiffersFromPerspective)
{
    Camera camera;
    Matrix4 perspective = camera.getProjectionMatrix(1.5f);

    camera.setProjection(Camera::Projection::Orthographic);
    Matrix4 orthographic = camera.getProjectionMatrix(1.5f);

    // The perspective divide term is the giveaway: it is -1 for perspective
    // and 0 for orthographic.
    EXPECT_NEAR(perspective.columns[2].w, -1.0f, EPSILON);
    EXPECT_NEAR(orthographic.columns[2].w, 0.0f, EPSILON);
}
