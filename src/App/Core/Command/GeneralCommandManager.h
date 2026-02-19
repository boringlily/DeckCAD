#pragma once
#include <optional>
#include <vector>
#include <variant>
#include <string>
#include <assert.h>

#include "Geometry.h"
#include "raylib.h"
#include "Command.h"
#include "GeneralCommand.h"
#include "SketchCommandManager.h"

/// Primary interface used to construct a sequence of CAD commands that are passed onto the kernel to generate geometry.
class GeneralCommandManager {
public:
    GeneralCommandManager()
    {
        sketch_commands.reserve(20);
        history.reserve(100);
    };

    SketchCommand& StartSketch()
    {
        SketchCommand& sketch = sketch_commands.emplace_back((static_cast<u32>(sketch_commands.size()) + 1));
        history.push_back(sketch.id);
        return sketch;
    };

private:
    SketchCommandManager sketch_manager;

    std::vector<SketchCommand> sketch_commands;
    std::vector<GeneralCommand::ID> history;
};
