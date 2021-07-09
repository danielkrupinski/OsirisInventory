#pragma once

#include <cstdint>

#include "Inconstructible.h"
#include "Pad.h"
#include "VirtualMethod.h"

class ModelInfo {
public:
    INCONSTRUCTIBLE(ModelInfo)

    VIRTUAL_METHOD(int, getModelIndex, WIN32_LINUX(2, 3), (const char* name), (this, name))
};
