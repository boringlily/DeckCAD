#pragma once
#include <vector>
#include <cinttypes>
#include <math.h>

using position = float;
using distance = float;
using degree = float;
using PointIndex = uint32_t;
using LineIndex = uint32_t;
using ArcIndex = uint32_t;

struct point2d
{
    position x{0};
    position y{0};
};

struct point3d : point2d
{
    position z{0};
};

namespace CustomMath
{
    distance point_distance(position x_1,position y_1, position x_2,position y_2 )
    {
        // Equation for getting distance between points d=√((x2 – x1)² + (y2 – y1)²).
        float difference_x = pow((x_2 - x_1), 2);
        float difference_y = pow((y_2 - y_1), 2);
        float sum_xy{difference_x+difference_y};
        return sqrt(sum_xy);
    }
}

// A Sketch represents a 2D drawing made up of lines and arcs which are 
// parametrically related via Geometric and Dimensional constraints.
// All sketches exist on a cartesian XY-plane and use a 3DPlane to position the 2D Sketch in 3D space.
class Sketch
{
public:

enum element_type
{
    NONE,
    POINT,
    LINE,
    ARC
};

// A special type for externally referring to elements of the sketch
struct GeometryID
{
    element_type type: 2;
    uint32_t index: 30;
};

static_assert(sizeof(GeometryID)==4);

struct SketchPoint
{
    position x;
    position y;
};

struct SketchLine
{
    PointIndex start;
    PointIndex end;
};

struct SketchArc
{
    PointIndex start;
    PointIndex end;
    PointIndex center;
};

    enum class GeometricConstraints : uint8_t
    {
        Horizontal, // Line vector can only point in ±X 
        Vertical, // Line vector can only point in ±Y 
        Perpendicular, // L1 is 90 deg from L2
        Midpoint, // Point snaps to middle of line.
        Parallel, // Lines can never touch
        Collinear, // Lines exist on same axis
        Symmetric, // Elements are identicall and mirrored across axis.
        Coincident, // P1 and P2 are snapped together.
        Concentric, // Arc1 and Arc2 have same center.
        Equal, // All dimentions are shared.
        Fixed // Position and Size can't change.
    };

    enum class DimensionConstraints : uint8_t
    {
        LineLength,
        LineAngle,
        ArcRadius,
        ArcAngle
    };

    using Selection = std::array<GeometryID, 2>;

    bool addLine(point2d start, point2d end);
    bool addArc(point2d start, point2d center, point2d end);
    
    bool addDimension(DimensionConstraints type, distance expression, Selection select);
    bool addConstraint(GeometricConstraints type, Selection select);

private:
std::vector<SketchPoint> points; 
std::vector<SketchLine> lines; 
std::vector<SketchArc> arcs; 


};





