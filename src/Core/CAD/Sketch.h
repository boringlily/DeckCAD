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
    SketchLineCommand() = delete;
};

class SketchCommand {
public:
    SketchCommand() = delete;
    SketchCommand(std::string name, Geometry::Plane3d plane) {};
    std::vector<iSketchGeometry> geometry;

    // SketchLineCommand& CreateLine()
    // {
    // };

    std::string name;
    Geometry::Plane3d plane;
};