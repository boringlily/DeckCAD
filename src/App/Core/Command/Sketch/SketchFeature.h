#pragma once
#include "ICommand.h"

enum class SketchCommandType {
    Line,
    Arc,
    Circle,
    Dimension,
};

using ISketchCommand
    = ICommand<SketchCommandType>;

class SketchLineCommand : public ISketchCommand {
    virtual bool IsValid() override
    {
        return false;
    }
};

class SketchArcCommand : public ISketchCommand {
    virtual bool IsValid() override
    {
        return false;
    }
};

class SketchCircleCommand : public ISketchCommand {
    virtual bool IsValid() override
    {
        return false;
    }
};

class SketchDimensionCommand : public ISketchCommand {
    virtual bool IsValid() override
    {
        return false;
    }
};

class SketchFeature : public CommandVariant<SketchCommandType, SketchLineCommand, SketchArcCommand, SketchCircleCommand, SketchDimensionCommand> {
};
