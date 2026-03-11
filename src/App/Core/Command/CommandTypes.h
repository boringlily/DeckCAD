#include "DTL.h"

class CommandType {
public:
    enum Type : u32 {
        NONE = 0,

        // Assembly Commands
        _AssemblyCommandsBegin = 0,

        _AssemblyCommandsEnd,

        // Part Commands
        _PartCommandsBegin = 0x2000u,

        CreateSketch,
        ExtrudeProfile,

        _PartCommandsEnd,

        // Sketch Commands
        _SketchCommandsBegin = 0x4000u,

        _SketchCommandsEnd,

        COMMAND_COUNT
    } type;

    bool IsSketchCommand() const
    {
        return type > Type::_SketchCommandsBegin && type < Type::_SketchCommandsEnd;
    }

    bool IsPartCommand() const
    {
        return type > Type::_PartCommandsBegin && type < Type::_PartCommandsEnd;
    }

    bool IsAssemblyCommand() const
    {
        return type > Type::_AssemblyCommandsBegin && type < Type::_AssemblyCommandsEnd;
    }

    bool IsCommandValid() const
    {
        return IsAssemblyCommand() || IsPartCommand() || IsSketchCommand();
    }

    overload operator()() const
    {
        return type;
    }

    overload operator=(Type t)
    {
        type = t;
    }
};

static_assert(sizeof(CommandType) == sizeof(u32), "CommandType should be 4 bytes in size.");