//
// RT64
//

#pragma once

#include "shared/rt64_hlsl.h"

#ifdef HLSL_CPU
namespace interop {
#endif
    struct RenderTargetCopyCB {
        uint usesHDR;
    };
#ifdef HLSL_CPU
};
#endif