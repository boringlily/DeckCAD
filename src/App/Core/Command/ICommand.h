#pragma once
#include "DTL.h"

using CommandId = u32;

template <typename CommandEnumType>
class ICommand {
public:
    ICommand() = delete;
    ICommand(CommandEnumType type, CommandId id)
        : type { type }
        , id { id } {};

    virtual bool IsValid() = 0;

    const CommandEnumType type;
    const CommandId id;
};

template <typename CommandType, typename... variant_types>
requires(std::derived_from<variant_types, ICommand<CommandType>>&&...) class CommandVariant {
public:
    CommandVariant(std::variant<variant_types...> variant)
        : variant { variant } {};

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
        return std::visit([](ICommand<CommandType>& val) { return val.type; }, variant);
    }

    bool IsType(CommandType type)
    {
        return type == GetType();
    }

private:
    DTL::Variant<variant_types...> variant;
};

enum class PartCommandType {
    CreateSketch,
    Extrude,
};

using IPartCommand
    = ICommand<PartCommandType>;

class CreateSketchCommand : public IPartCommand {
    virtual bool IsValid()
    {
        return false;
    }
};

class PartFeature : public CommandVariant<PartCommandType, CreateSketchCommand> {
};