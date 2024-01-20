//
// RT64
//

#pragma once

#include "common/rt64_common.h"

#include "../include/rt64_extended_gbi.h"

namespace RT64 {
    struct TransformGroup {
        uint32_t matrixId = G_EX_ID_AUTO;
        bool decompose = true;
        uint8_t positionInterpolation = G_EX_COMPONENT_AUTO;
        uint8_t rotationInterpolation = G_EX_COMPONENT_AUTO;
        uint8_t scaleInterpolation = G_EX_COMPONENT_AUTO;
        uint8_t skewInterpolation = G_EX_COMPONENT_AUTO;
        uint8_t perspectiveInterpolation = G_EX_COMPONENT_AUTO;
        uint8_t vertexInterpolation = G_EX_COMPONENT_SKIP;
        uint8_t tileInterpolation = G_EX_COMPONENT_AUTO;
        uint8_t ordering = G_EX_ORDER_AUTO;
        uint8_t editable = G_EX_EDIT_NONE;
    };
};