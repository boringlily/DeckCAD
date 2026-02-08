#pragma once
#include "Geometry.h"
#include "Command.h"
#include <expected>

enum SketchGeometryType {
    LineSegment,
    ArcSegment,
    Circle
};

class SketchLineCommand {
public:
    SketchLineCommand() = delete;

    std::vector<Geometry::Point2> points;

    void AddPoint()
    {
    }

    void RemoveLastPoint()
    {
    }

    void UpdateLastPoint()
    {
    }
};

class SketchCommandManager : GeneralCommand {
public:
    SketchCommandManager() = default;
    SketchCommandManager(std::string name, Geometry::Plane3d plane)
        : name { std::move(name) }
        , plane { std::move(plane) } {};

    std::vector<SketchCommandId> history;

    // std::expected<SketchLineCommand&> CreateLine
    // {

    // };

    CommandParameter<std::string> name;
    CommandParameter<Geometry::Plane3d> plane;

    virtual bool IsValid() override
    {
        return (name.IsSet() && plane.IsSet());
    }
};