//
// RT64
//

#pragma once

#include "shared/rt64_hlsl.h"

#ifdef HLSL_CPU
namespace interop {
#endif
#ifdef HLSL_CPU
    union GPUTileFlags {
        struct {
            uint alphaIsCvg : 1;
            uint highRes : 1;
            uint fromCopy : 1;
            uint rawTMEM : 1;
            uint hasMipmaps : 1;
        };

        uint value;
    };
#else
    // SPIR-V code generation does not seem to like bitfields at the moment, so we work around it by querying the flags manually.
    typedef uint GPUTileFlags;

    bool gpuTileFlagAlphaIsCvg(GPUTileFlags flags) {
        return flags & 0x1;
    }

    bool gpuTileFlagHighRes(GPUTileFlags flags) {
        return flags & 0x2;
    }

    bool gpuTileFlagFromCopy(GPUTileFlags flags) {
        return flags & 0x4;
    }

    bool gpuTileFlagRawTMEM(GPUTileFlags flags) {
        return flags & 0x8;
    }

    bool gpuTileFlagHasMipmaps(GPUTileFlags flags) {
        return flags & 0x10;
    }
#endif
    struct GPUTile {
        float2 ulScale;
        float2 tcScale;
        uint2 texelShift;
        uint2 texelMask;
        uint textureIndex;
        GPUTileFlags flags;
    };
#ifdef HLSL_CPU
};
#endif