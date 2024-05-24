//
// RT64
//

#pragma once

#include "shared/rt64_hlsl.h"

#ifdef HLSL_CPU
namespace interop {
#endif
    struct FrameParams {
        uint frameCount;
        uint viewUbershaders;
        float ditherNoiseStrength;
    };
#ifdef HLSL_CPU
};
#endif