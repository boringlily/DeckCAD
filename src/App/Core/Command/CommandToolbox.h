#pragma once
#include "PartCommandToolbox.h"

class CommandToolbox {
public:
    PartCommandToolbox part_toolbox;

    bool IsSketchContext()
    {
        return part_toolbox.IsCommandActive() && part_toolbox.GetActiveCommand().IsType(PartCommandType::CreateSketch);
    }

    bool IsSketchCommandActive()
    {
        return IsSketchContext() && part_toolbox.GetActiveCommand().As<CreateSketchCommand>()->IsCommandActive();
    }
}