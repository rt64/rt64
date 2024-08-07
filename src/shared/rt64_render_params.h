//
// RT64
//

#pragma once

#include "shared/rt64_hlsl.h"

#ifdef HLSL_CPU
#include "shared/rt64_render_flags.h"

namespace interop {
#endif
#ifndef HLSL_CPU
    typedef uint RenderFlags;
#endif

    struct RenderParams {
        uint ccL;
        uint ccH;
        uint omL;
        uint omH;
        RenderFlags flags;
    };
#ifdef HLSL_CPU
};
#endif