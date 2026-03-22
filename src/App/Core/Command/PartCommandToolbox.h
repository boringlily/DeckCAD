#pragma once
#include "PartFeature.h"
#include "ICommandToolbox.h"

class PartCommandToolbox : public ICommandToolbox<PartCommandType, PartFeature> {
};