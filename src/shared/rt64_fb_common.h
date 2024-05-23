//
// RT64
//

#pragma once

#include "shared/rt64_hlsl.h"

#define FB_COMMON_WORKGROUP_SIZE 8

#ifdef HLSL_CPU
namespace interop {
#endif
    struct FbCommonCB {
        uint2 offset;
        uint2 resolution;
        uint fmt;
        uint siz;
        uint ditherPattern;
        uint ditherRandomSeed;
        uint usesHDR;
    };
#ifdef HLSL_CPU
};
#endif