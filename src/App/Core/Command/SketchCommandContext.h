#pragma once
#include <optional>
#include <variant>
#include "CommandHistory.h"
#include "SketchCommand.h"

class SketchCommandContext {
public:
    SketchCommandContext() = delete;
    SketchCommandContext(CommandHistory& history)
        : history { history }
    {
        commands.reserve(100);
    };

    // If context not active begin one, other wise do nothing. Returns true if context is active after call.
    bool BeginSketchContext(GeneralCommand active_create_sketch_command)
    {
        if (!IsContextActive() && active_create_sketch_command.type == GeneralCommandType::CreateSketch && active_create_sketch_command.IsValid()) {
            // Reset state
            active_command = std::nullopt;
            commands.clear();

            active_context = true;
            context_id = active_create_sketch_command.id;

            // Check the history if there are already commands for the given context_id, and if so, load them into the context.
            if (context_id - 1u < history.sketch_command_slices.size()) {
                SketchCommandSlice slice = history.sketch_command_slices[context_id - 1u];

                auto iterator_begin = history.sketch_history.begin() + slice.command_offset;
                auto iterator_end = iterator_begin + slice.command_count;

                commands.insert(commands.end(), iterator_begin, iterator_end);
            }
        }

        return IsContextActive();
    }

    /// @brief If context active, and no sketch command active, end context, other wise do nothing. Returns true if context is not active after call.
    bool EndSketchContext()
    {
        if (IsContextActive() && !active_command.has_value()) {
            // Save sketch context commands to history.
            history.SaveSketchContextCommands(context_id, commands);

            // Reset state
            active_command = std::nullopt;
            commands.clear();

            active_context = false;
            context_id = 0;
            return true;
        }

        return false;
    }

    bool FinishCommand()
    {
        if (IsContextActive() && active_command.has_value() && active_command->IsValid()) {
            commands.push_back(active_command.value());
            active_command.reset();
            return true;
        }

        return false;
    }

    bool CancelCommand()
    {
        if (IsContextActive() && active_command.has_value()) {
            active_command.reset();
            return true;
        }

        return false;
    }

    inline bool IsContextActive() const
    {
        return active_context && context_id != 0;
    }

private:
    std::vector<SketchCommand> commands;
    std::optional<SketchCommand> active_command { std::nullopt };

    // If this doesn't have value, we don't have an active context.

    u32 context_id { 0 };
    bool active_context { false };
    CommandHistory& history;
};