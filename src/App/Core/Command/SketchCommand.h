#pragma once
#include <Command.h>
#include <Geometry.h>

enum class SketchCommandType : u32 {
    None = 0,

    // Geometric Commands
    Line,

    // Dimensional Constraints

    // Geometric Constraints

    COMMAND_COUNT
};

class SketchCommand : public Command<SketchCommandType> {
public:
    std::optional<Geometry::Point2> p1, p2;
};
