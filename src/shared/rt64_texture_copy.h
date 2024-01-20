//
// RT64
//

#pragma once

#include "shared/rt64_hlsl.h"

#ifdef HLSL_CPU
namespace interop {
#endif
    struct TextureCopyCB {
        float2 uvScroll;
        float2 uvScale;
    };
#ifdef HLSL_CPU
};
#endif