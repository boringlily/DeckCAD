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
    bool StartCommand()
    {
        FeatureVariant temp { T { history.size() } };
        history.push_back(temp);
    }

protected:
    FeatureVariant* active_command { nullptr };

    DTL::List<FeatureVariant> history;
};

class CommandManager : public ICommandManager<PartCommandType, PartFeature> {
};

// class CommandManager {
// public:
//     bool StartCommand(CommandType type)
//     {
//     }

//     void CancelCommand()
//     {
//     }

//     bool FinishCommand()
//     {
//     }

// private:
//     DTL::List<PartFeature> history;
// };