#pragma once
#include "DTL.h"
#include "CommandTypes.h"

class FeatureCommand {
    FeatureCommand() = delete;
    FeatureCommand(CommandType type)
        : type { type } {};

public:
    const CommandType type { CommandType::NONE };

    virtual bool IsValid() = 0;
};