#pragma once
#include <optional>
#include <vector>
#include <variant>
#include <string>
#include <assert.h>

#include "Geometry.h"
#include "raylib.h"
#include "Command.h"
#include "SketchCommandManager.h"

/// Primary interface used to construct a sequence of CAD commands that are passed onto the kernel to generate geometry.
class GeneralCommandManager {

public:
    GeneralCommandManager() {};

    SketchCommandManager& StartSketch()
    {
        history.emplace_back(GeneralCommandId::Type::StartSketch, sketch_list.size());
        return sketch_list.emplace_back();
    };

private:
    std::vector<SketchCommandManager> sketch_list;
    std::vector<GeneralCommandId> history;
};
