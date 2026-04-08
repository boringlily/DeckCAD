#pragma once
#include "PartFeature.h"
#include "SketchFeature.h"

class CommandToolbox {
public:
    bool IsSketchContext()
    {
        return sketch_command.has_value() || (IsPartCommandActive() && part_command.value().IsType(PartCommandType::CreateSketch));
    }

    bool IsSketchCommandActive()
    {
        return sketch_command.has_value();
    }

    bool IsPartCommandActive()
    {
        return part_command.has_value();
    }

    bool StartSketchCommand(SketchCommandType type)
    {
    }

    bool StartPartCommand(PartCommandType type)
    {
    }

    void CancelSketchCommand()
    {
    }

private:
    std::optional<PartFeature> part_command { std::nullopt };
    std::optional<SketchFeature> sketch_command { std::nullopt };

    std::vector<PartFeature> part_history;
};