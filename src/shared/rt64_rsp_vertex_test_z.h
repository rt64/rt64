//
// RT64
//

#pragma once

#include "shared/rt64_hlsl.h"

#ifdef HLSL_CPU
namespace interop {
#endif
    struct RSPVertexTestZCB {
        float2 resolutionScale;
        uint vertexIndex;
        uint srcIndexStart;
        uint dstIndexStart;
        uint indexCount;
    };
#ifdef HLSL_CPU
};
#endif