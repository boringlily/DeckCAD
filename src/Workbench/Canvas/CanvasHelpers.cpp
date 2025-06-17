#include "GeometryMath.h"

float canvas_scale{1.0F};
float grid_spacing_mm = 5;
constexpr distance GRID_SPACING_MIN{0.001};
constexpr distance GRID_SPACING_MAX{1000};

struct Plane3D
{
    Vector3 center;
    float angle_x;
    float angle_y;
    Vector2 size{20, 20};
};

namespace UI
{
    enum class OriginPlane
    {
        XY,
        XZ,
        YZ
    };

    void DrawOriginPlane(OriginPlane plane, Vector3 centerPos, Vector2 size, Color color)
    {
        rlPushMatrix();
            rlTranslatef(centerPos.x, centerPos.y, centerPos.z);

            switch(plane)
            {
                case OriginPlane::XY:
                    rlRotatef(90, 1, 0, 0);
                break;
                case OriginPlane::XZ:
                    // Do nothing, already the home position
                break;
                case OriginPlane::YZ:
                    rlRotatef(90, 0, 0, -1);
                break;
                default:
                
                // If this condition is hit, crash.
                assert(false);
            }
            rlScalef(size.x, 1.0f, size.y);
            rlBegin(RL_QUADS);
                rlColor4ub(color.r, color.g, color.b, color.a);
                rlNormal3f(0.0f, 1.0f, 0.0f);

                rlVertex3f(-0.5f, 0.0f, -0.5f);
                rlVertex3f(-0.5f, 0.0f, 0.5f);
                rlVertex3f(0.5f, 0.0f, 0.5f);
                rlVertex3f(0.5f, 0.0f, -0.5f);
            rlEnd();
        rlPopMatrix();
    };

    void DrawPlane(Plane3D plane)
    {
        rlPushMatrix();
            rlTranslatef(plane.center.x, plane.center.y, plane.center.z);
            rlRotatef(plane.angle_x, 1, 0, 0);
            rlRotatef(plane.angle_y, 0, 0, -1);

            rlScalef(plane.size.x, 1.0f, plane.size.y);
            rlBegin(RL_QUADS);
                rlColor4ub(255, 203, 0, 20);
                rlNormal3f(0.0f, 1.0f, 0.0f);

                rlVertex3f(-0.5f, 0.0f, -0.5f);
                rlVertex3f(-0.5f, 0.0f, 0.5f);
                rlVertex3f(0.5f, 0.0f, 0.5f);
                rlVertex3f(0.5f, 0.0f, -0.5f);
            rlEnd();
        rlPopMatrix();

    }
    
    void DrawGrid(OriginPlane plane, int slices, float spacing)
    {
        int halfSlices = slices / 2;
        rlPushMatrix();
            switch(plane)
            {
                case OriginPlane::XY:
                    rlRotatef(90, 1, 0, 0);
                break;
                case OriginPlane::XZ:
                    // Do nothing, already the home position
                break;
                case OriginPlane::YZ:
                    rlRotatef(90, 0, 0, -1);
                break;
                default:
                
                // If this condition is hit, crash.
                assert(false);
            }
            rlBegin(RL_LINES);
                for (int i = -halfSlices; i <= halfSlices; i++)
                {
                    if (i == 0)
                    {
                        rlColor3f(0.5f, 0.5f, 0.5f);
                    }
                    else
                    {
                        rlColor3f(0.75f, 0.75f, 0.75f);
                    }

                    rlVertex3f(static_cast<float>(i) * spacing, 0.0f, static_cast<float>(-halfSlices) * spacing);
                    rlVertex3f(static_cast<float>(i)  * spacing, 0.0f, static_cast<float>(halfSlices) * spacing);

                    rlVertex3f(static_cast<float>(-halfSlices) * spacing, 0.0f, static_cast<float>(i) * spacing);
                    rlVertex3f(static_cast<float>(halfSlices) * spacing, 0.0f, static_cast<float>(i) * spacing);
                }
            rlEnd();
        rlPopMatrix();
    }
}


