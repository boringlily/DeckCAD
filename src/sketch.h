#pragma once
#include <vector>
#include "raylib.h"
#include "geometry_math.h"

// Type specificity for sketch element indexing.
using PointIndex = uint32_t;
using LineIndex = uint32_t;
using ArcIndex = uint32_t;

struct SketchPoint
{
    position x;
    position y;

    SketchPoint(Vector2 &point):x{point.x},y{point.y}{};
};

enum class PointName : uint8_t
{
    A = 0,
    B,
    C,
    // D,
    // E,
    // F,
    // G,

// Alias
    Start = A,
    End = B, 
    Center = C,
    Midpoint = C,
};

class SketchElement
{
    public:
    enum Type
    {
        Line,
        Circle,
        Arc
    };

    // Make a Line
    SketchElement(Vector2 start, Vector2 end):geometryType{Type::LINE}{};
    
    // Arc
    SketchElement(Vector2 ArcStart, Vector2 ArcCenter, Vector2 ArcEnd):geometryType{Type::LINE}{};
    
    // Cirle 
    SketchElement(Vector2 circle_center, distance radius):geometryType{Type::LINE}{};

    SketchPoint & get_point(PointName point);

    protected:

    const Type geometryType;

    private:
    std::array<Vector2&, 3> points;
    distance size;
    degree angle;
};

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

// A Sketch represents a 2D drawing made up of lines and arcs which are
// parametrically related via Geometric and Dimensional constraints.
// All sketches exist on a cartesian XY-plane and use a 3DPlane to position the Sketch in 3D space.
class Sketch
{
public:

    Sketch(){};

    bool addLineFromPoints(Vector2 start, Vector2 end);
    bool addLine(Vector2 start, distance length, degree angle = 0);

    bool addArcFrom3Points(Vector2 start, Vector2 center, Vector2 end); // Doesn't guarantee arc is following the path correctly.
    // bool addArcFrom2Points(Vector2 start, Vector2 center, degree arc_angle);
    // bool addArcCenterRadius(Vector2 center, distance radius, degree arc_angle);
    // bool addCircle(Vector2 center, distance diameter);

    // bool deleteElement(GeometryID item_to_delete);

    void render();

private:
    SketchPointList ;

    ConstraintSolver solver{points,lines,arcs};

    Plane sketch_plane;
};
