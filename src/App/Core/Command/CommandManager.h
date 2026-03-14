#pragma once
#include "DTL.h"
#include "ICommand.h"

template <typename CommandEnumType, typename FeatureVariant>
class ICommandManager {
public:
    bool IsCommandActive()
    {
        return active_command != nullptr;
    }

    template <typename T>
    requires std::is_base_of_v<ICommand<CommandEnumType>, T>
    bool StartCommand<T>()
    {
    }

protected:
    FeatureVariant* active_command { nullptr };

    DTL::List<FeatureVariant> history;
}

class CommandManager {
public:
    bool StartCommand(CommandType type)
    {
    }

    void CancelCommand()
    {
    }

    bool FinishCommand()
    {
        if (context == CommandContext::Sketch) {
            if (active_sketch_command.has_value() && active_sketch_command->Valid()) {

            } else if (active_part_command().has_value() && active_part_command->Valid()) {
            }
            return false;
        } else if (context == CommandContext::Part && active_part_command.has_value()) {
        }

        return false;
    }

    CommandContext GetContext()
    {
        return context;
    }

private:
    DTL::List<PartFeature> history;
};