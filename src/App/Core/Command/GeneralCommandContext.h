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

class CommandHistory {
public:
    std::array<GeneralCommand, u16_max> general_history;
    std::array<SketchCommand, u16_max> sketch_history;

}

class CommandManager {
public:
    using ContextVariant = std::variant<GeneralCommandFactory, SketchCommandFactory>;

    void FinishCommand() {};
    void CancelCommand() {};

    ContextVariant& GetContextVariant()
    {
        return context_variant;
    }

    CommandContext GetContext()
    {
        return current_context;
    }

    CommandContext current_context { CommandContext::General };
    ContextVariant context_variant = GeneralCommandFactory();

    // General Context
    GeneralCommandFactory general_command_manager;

    // Sub-context that is only available when a GeneralCommands::StartSketchCommand is issued.
    SketchCommandFactory sketch_command_manager;
};
