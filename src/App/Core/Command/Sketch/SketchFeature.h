#pragma once
#include "ICommand.h"
#include "SketchCommandType.h"
#include "Geometry.h"

class SketchLineCommand : public ISketchCommand {
public:
    explicit SketchLineCommand(CommandId id)
        : ISketchCommand(SketchCommandType::Line, id) {};
    virtual bool IsValid() override
    {
        return false;
    }

    Geometry::Point2 start;
    Geometry::Point2 end;
};

class SketchArcCommand : public ISketchCommand {
public:
    explicit SketchArcCommand(CommandId id)
        : ISketchCommand(SketchCommandType::Arc, id) {};
    virtual bool IsValid() override
    {
        return false;
    }
};

class SketchCircleCommand : public ISketchCommand {
public:
    explicit SketchCircleCommand(CommandId id)
        : ISketchCommand(SketchCommandType::Circle, id) {};
    virtual bool IsValid() override
    {
        return false;
    }
};

class SketchDimensionCommand : public ISketchCommand {
public:
    explicit SketchDimensionCommand(CommandId id)
        : ISketchCommand(SketchCommandType::Dimension, id) {};
    virtual bool IsValid() override
    {
        return false;
    }
};

class SketchFeature : public CommandVariant<SketchCommandType, SketchLineCommand, SketchArcCommand, SketchCircleCommand, SketchDimensionCommand> {
};
