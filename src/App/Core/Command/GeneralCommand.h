#pragma once
#include <Command.h>
#include <Geometry.h>

enum class GeneralCommandType : u32 {
    None = 0,

    CreateSketch,
    Extrude,

    COMMAND_COUNT
};

class GeneralCommand : public Command<GeneralCommandType> {
public:
    Geometry::Plane3d plane;
};
