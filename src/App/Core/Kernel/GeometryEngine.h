#pragma once
#include "Sketch.h"
#include "CreateSketchCommand.h"

class GeometryEngine {
public:
    static Geometry::Sketch Evaluate(CreateSketchCommand& cmd);
};

inline Geometry::Sketch GeometryEngine::Evaluate(CreateSketchCommand& cmd)
{
    Geometry::Sketch sketch;
    sketch.plane = *cmd.plane;

    for (auto& feature : cmd.history) {
        if (!feature.IsType(SketchCommandType::Line))
            continue;
        auto* line = feature.As<SketchLineCommand>();
        if (!line || !line->start.has_value() || !line->end.has_value())
            continue;
        sketch.lines.push_back({ *line->start, *line->end });
    }

    return sketch;
}
