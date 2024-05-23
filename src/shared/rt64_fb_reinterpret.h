//
// RT64
//

#pragma once

#include "shared/rt64_hlsl.h"

#ifdef HLSL_CPU
namespace interop {
#endif
    struct FbReinterpretCB {
        uint2 resolution;
        float sampleScale;
        uint srcSiz;
        uint srcFmt;
        uint dstSiz;
        uint dstFmt;
        uint tlutFormat;
        uint ditherPattern;
        uint ditherRandomSeed;
        uint2 ditherOffset;
        uint usesHDR;
    };
#ifdef HLSL_CPU
};
#endif