#pragma once
#include "AppMemory.h"
#include "Core.export.h"

#ifdef __cplusplus
extern "C" {
#endif

CORE_API
void CoreInit(AppMemory& app);

CORE_API
void CoreUpdate();

#ifdef __cplusplus
}
#endif
