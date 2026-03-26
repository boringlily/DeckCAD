#pragma once
#include "ICommand.h"
#include "SketchCommandType.h"

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
