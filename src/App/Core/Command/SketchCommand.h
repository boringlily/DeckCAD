#pragma once
#include <Command.h>

class SketchCommand : public Command {
public:
    using Count = u32;
    enum Type : u32 {
        None = 0,
        // Geometric Commands
        Line,

        //
    };

    Type type { None };
    Count count { 0 };
};