#pragma once
#include "DTL.h"
#include "ICommand.h"

template <typename CommandEnumType, typename FeatureVariant>
class ICommandToolbox {
public:
    bool IsCommandActive()
    {
        return active_command != nullptr;
    }

    template <typename T>
    requires std::is_base_of_v<ICommand<CommandEnumType>, T>
    bool StartCommand()
    {
        if (active_command != nullptr) {
            return false;
        }

        FeatureVariant temp { T { command_counter } };
        history.push_back(temp);
        active_command = history.back();

        return true;
    }

    /// @brief Returns reference to the active command. Caller must ensure a command is active before calling this function.
    /// @pre IsCommandActive() == true
    /// @return Reference to the active command or crash if no command is active.
    FeatureVariant& GetActiveCommand()
    {
        assert(active_command != nullptr);
        return *active_command;
    }

    bool FinishCommand()
    {
        if (active_command != nullptr && active_command->IsValid()) {
            // When command is finished, we increment id for the next command
            command_counter++;
            active_command = nullptr;
            return true;
        }
        return false;
    }

    bool CancelCommand()
    {
        if (active_command != nullptr) {
            history.pop_back();
            active_command = nullptr;
            return true;
        }
        return false;
    }

protected:
    u32 command_counter { 1 };
    FeatureVariant* active_command { nullptr };
    DTL::List<FeatureVariant> history;
};
