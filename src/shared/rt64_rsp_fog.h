//
// RT64
//

#pragma once

#include "shared/rt64_hlsl.h"

#ifdef HLSL_CPU
namespace interop {
#endif
    struct RSPFog {
        float mul;
        float offset;
    };
#ifdef HLSL_CPU
};
#endif