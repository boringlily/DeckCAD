#pragma once
#include "DTL.h"
#include "CommandTypes.h"

using CommandId = u32;

template <typename EnumType>
class ICommand {
public:
    ICommand() = delete;
    ICommand(CommandType type, CommandId id)
        : type { type }
        , id { id } {};

    virtual bool IsValid() = 0;

    const EnumType type;
    const CommandId id;
}

template <typename CommandType, typename... variant_types>
requires(std::derived_from<variant_types, ICommand<CommandType>>&&...) class CommandVariant {
public:
    CommandVariant()

        using Command = ICommand<CommandType>;

    template <typename T>
    requires std::is_base_of_v<ICommand<CommandType>, T>
        T* As()
    {
        T* v = dynamic_cast<T*>(visit([](ICommand<CommandType>& val) { return &val; }, variant));
        return v;
    };

    bool IsValid()
    {
        return std::visit([](ICommand<CommandType>& val) { return val.IsValid(); }, variant);
    }

    CommandType GetType()
    {
        return std::visit([](Command& val) { return val.type; }, variant);
    }

    bool IsType(CommandType type)
    {
        return type == GetType();
    }

private:
    DTL::Variant<variant_types...> variant;
}

enum class PartCommandType {
    CreateSketch,
    Extrude,
};

using IPartCommand
    = ICommand<PartCommandType>;

class PartFeature : public CommandVariant<> {
};