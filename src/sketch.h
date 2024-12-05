#pragma once
#include <vector>
#include "geometry_math.h"

// Type specificity for sketch element indexing.
using PointIndex = uint32_t;
using LineIndex = uint32_t;
using ArcIndex = uint32_t;

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

using SketchPointList = std::vector<SketchPoint>;
using SketchLineList = std::vector<SketchLine>;
using SketchArcList = std::vector<SketchArc>;

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
    enum
    {
        NONE,
        POINT,
        LINE,
        ARC
    } type : 2;
    uint32_t index : 30;
};
static_assert(sizeof(GeometryID) == 4);

enum class GeometricConstraints : uint8_t
{
    Horizontal,    // Line vector can only point in ±X
    Vertical,      // Line vector can only point in ±Y
    Perpendicular, // L1 is 90 deg from L2
    Midpoint,      // Point snaps to middle of line.
    Parallel,      // Lines can never touch
    Collinear,     // Lines exist on same axis
    Symmetric,     // Elements are identicall and mirrored across axis.
    Coincident,    // P1 and P2 are snapped together.
    Concentric,    // Arc1 and Arc2 have same center.
    Equal,         // All dimentions are shared.
    Fixed          // Position and Size can't change.
};

enum class DimensionConstraints : uint8_t
{
    LineLength,
    LineAngle,
    ArcRadius,
    ArcAngle
};

class ConstraintSolver
{
public:
    ConstraintSolver(
        SketchPointList &sketch_points,
        SketchLineList &sketch_lines,
        SketchArcList &sketch_arcs) : sketch_points{sketch_points}, sketch_lines{sketch_lines}, sketch_arcs{sketch_arcs} {};

    using Selection = std::array<GeometryID, 2>;

    bool addDimension(DimensionConstraints type, distance expression, Selection select);
    bool addConstraint(GeometricConstraints type, Selection select);

private:
    struct constraint
    {
        PointIndex P1,P2;
        LineIndex L1,L2;
        ArcIndex A1,A2;
    };

    SketchPointList &sketch_points;
    SketchLineList &sketch_lines;
    SketchArcList &sketch_arcs;
};

// A Sketch represents a 2D drawing made up of lines and arcs which are
// parametrically related via Geometric and Dimensional constraints.
// All sketches exist on a cartesian XY-plane and use a 3DPlane to position the Sketch in 3D space.
class Sketch
{
public:
    bool addLineFromPoints(point2d start, point2d end);
    bool addLine(point2d start, distance length, degree angle = 0);

    bool addArcFrom3Points(point2d start, point2d center, point2d end); // Doesn't guarantee arc is following the path correctly.
    bool addArcFrom2Points(point2d start, point2d center, degree arc_angle);
    bool addArcCenterRadius(point2d center, distance radius, degree arc_angle);
    bool addCircle(point2d center, distance diameter);

    bool deleteElement(GeometryID item_to_delete);

private:
    SketchPointList points;
    SketchLineList lines;
    SketchArcList arcs;

    ConstraintSolver solver{points,lines,arcs};
};
