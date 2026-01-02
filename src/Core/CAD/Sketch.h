#pragma once
#include "Geometry.h"

enum SketchGeometryType {
    LineSegment,
    ArcSegment,
    Circle
};

using PointId = u32;

class iSketchGeometry {
    iSketchGeometry() = delete;
    iSketchGeometry(SketchGeometryType type)
        : type { type } {};
    SketchGeometryType type;
};

class SketchLineCommand {
public:
    SketchLine() = delete;
};

class SketchCommand {
public:
    Sketch() = delete;
    Sketch(std::string name, Geometry::Plane3d plane) {};
    std::vector<iSketchGeometry> geometry;

    SketchLine& CreateLine()
    {
    }

    std::string name;
    Geometry::Plane3d plane;
};