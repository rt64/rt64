//
// RT64
//

#pragma once

#include "shared/rt64_hlsl.h"

#ifdef HLSL_CPU
namespace interop {
#endif
    struct FramebufferParams {
        float2 resolution;
        float2 resolutionScale;
        float horizontalMisalignment;
    };
#ifdef HLSL_CPU
};
#endif