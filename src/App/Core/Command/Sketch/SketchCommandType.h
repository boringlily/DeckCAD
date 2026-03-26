#pragma once
#include "ICommand.h"

enum class SketchCommandType {
    Line,
    Arc,
    Circle,
    Dimension,
};

using ISketchCommand = ICommand<SketchCommandType>;
