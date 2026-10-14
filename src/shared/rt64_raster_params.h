//
// RT64
//

#pragma once

#include "shared/rt64_hlsl.h"

#ifdef HLSL_CPU
namespace interop {
#endif
    struct RasterParams {
        uint renderIndex;
        uint _pad0;
        uint _pad1;
        uint _pad2;
        float2 screenScale;
        float2 screenOffset;
    };
#ifdef HLSL_CPU
};
#endif