#pragma once
#include "PartCommandType.h"
#include "CreateSketchCommand.h"
#include "Sketch/SketchCommandType.h"

class PartFeature : public CommandVariant<PartCommandType, CreateSketchCommand> {
};