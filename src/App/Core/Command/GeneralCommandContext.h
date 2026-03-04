#pragma once
#include <optional>
#include <vector>
#include <array>
#include <variant>
#include <string>
#include <assert.h>

#include "Geometry.h"
#include "raylib.h"
#include "Command.h"
#include "GeneralCommand.h"
#include "SketchCommand.h"

enum class CommandContext {
    General,
    Sketch
};

class GeneralCommandContext {
public:
    explicit GeneralCommandContext(CommandHistory& history)
        : history { history } {};

    void StartSketchCommand()
    {
        GeneralCommand command { GeneralCommandType::CreateSketch };

        history.active_general_command = command;
    }

    // Count of every command issued for the active history.
private:
    CommandHistory& history;
};
