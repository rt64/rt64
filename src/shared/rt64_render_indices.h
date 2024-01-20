//
// RT64
//

#pragma once

#include "shared/rt64_hlsl.h"

#ifdef HLSL_CPU
namespace interop {
#endif
    struct RenderIndices {
        uint instanceIndex;
        uint faceIndicesStart;
        uint rdpTileIndex;
        uint rdpTileCount;
        uint highlightColor;
    };
#ifdef HLSL_CPU
};
#endif