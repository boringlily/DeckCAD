#pragma once
#include "ICommand.h"
#include "Sketch/SketchCommandToolbox.h"

enum class PartCommandType {
    CreateSketch,
    Extrude,
};

using IPartCommand
    = ICommand<PartCommandType>;

class CreateSketchCommand : public IPartCommand, public SketchCommandToolbox {
    virtual bool IsValid()
    {
        return false;
    }
};

class PartFeature : public CommandVariant<PartCommandType, CreateSketchCommand> {
};