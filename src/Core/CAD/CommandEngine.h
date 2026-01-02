#pragma once
#include <optional>
#include <vector>
#include <variant>
#include <string>

#include "Geometry.h"
#include "raylib.h"
#include "Sketch.h"

/// @brief Command type identifier
enum CommandType : u32 {
    Invalid = 0,
    CreateSketch,
    Total
};

struct Command {
    Command() = delete;
    Command(CommandType type, u32 index)
        : type { type }
        , index { index } {};

    const CommandType type;
    const u32 index;
};

static_assert(sizeof(Command) == sizeof(u64));

/// Primary interface used to construct a sequence of CAD commands that are passed onto the kernel to generate geometry.
class GeometryCommander {
public:
    GeometryCommander() {};

public: // Commands
private:
    std::vector<Sketch> sketches;
    std::vector<Command> history;
};