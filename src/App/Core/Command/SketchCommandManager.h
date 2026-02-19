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

class SketchContextCommand : public GeneralCommand {
public:
    SketchContextCommand() = delete;
    SketchContextCommand(u32 sketch_number)
        : GeneralCommand(GeneralCommand::ID(GeneralCommand::ID::Type::StartSketch, sketch_number)) {};

    CommandParameter<std::string> name;
    CommandParameter<Geometry::Plane3d> plane;

    virtual bool IsValid() override
    {
        return (name.IsSet() && plane.IsSet());
    }

    bool IsActive()
    {
        return is_active;
    }

private:
    bool is_active { false };
};

class SketchCommandManager {
public:
    SketchCommandManager() = default;

    bool BeginSketchContext()
    {
    }

    bool EndSketchContext()
    {
    }

private:
};