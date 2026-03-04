#pragma once
#include "CommandHistory.h"
#include "GeneralCommandContext.h"
#include "SketchCommandContext.h"

class CommandManager {
public:
    using ContextVariant = std::variant<GeneralCommandContext, SketchCommandContext>;

    bool IsSketchContextActive()
    {
        return active_context == CommandContext::Sketch;
    }

    bool IsGeneralContextActive()
    {
        return active_context == CommandContext::General;
    }

    CommandHistory history {};

    CommandContext active_context { CommandContext::General };

    GeneralCommandContext general_context { history };

    std::optional<SketchCommandContext> sketch_context;
};