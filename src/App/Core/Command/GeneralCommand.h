#pragma once
#include <Command.h>

class GeneralCommand : public Command {
public:
    class ID {
    public:
        using Count = u32;
        enum Type : u32 {
            Invalid = 0,

            StartSketch,
            Extrude,

            GeneralCommandEnd,
        };

        Type type { Invalid };
        Count count { 0 };

        explicit ID() = delete;
        explicit ID(Type type, Count count)
            : type { type }
            , count { count }
        {
            assert(IsValid() && "Command ID has been constructed with out-of-range parameters and is now considered invalid.");
        };

        bool IsValid()
        {
            return type > Invalid && type < GeneralCommandEnd && count > 0;
        };
    };

    explicit GeneralCommand(GeneralCommand::ID& id)
        : id { std::move(id) } {};
    explicit GeneralCommand(GeneralCommand::ID id)
        : id { id } {};

    const ID id;
};
