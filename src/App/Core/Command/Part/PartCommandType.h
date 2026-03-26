#pragma once
#include "ICommand.h"

enum class PartCommandType {
    CreateSketch,
    Extrude,
};

using IPartCommand = ICommand<PartCommandType>;