//
// RT64
//

#pragma once

#include "shared/rt64_hlsl.h"

#ifdef HLSL_CPU
namespace interop {
#endif
#ifdef HLSL_CPU
    union RenderFlags {
        struct {
            uint rect : 1;
            uint NoN : 1;
            uint culling : 1;
            uint smoothShade : 1;
            uint linearFiltering : 1;
            uint smoothNormal : 1;
            uint normalMap : 1;
            uint shadowAlpha : 1;
            uint oneCycleHardwareBug : 1;
            uint blenderApproximation : 2;
            uint dynamicTiles : 1;
            uint canDecodeTMEM : 1;
            uint cms0 : 2;
            uint cmt0 : 2;
            uint cms1 : 2;
            uint cmt1 : 2;
            uint usesTexture0 : 1;
            uint usesTexture1 : 1;
            uint upscale2D : 1;
            uint upscaleLOD : 1;
            uint usesHDR : 1;
        };

        uint value;
    };
#else
    // SPIR-V code generation does not seem to like bitfields at the moment, so we work around it by querying the flags manually.
    typedef uint RenderFlags;

    bool renderFlagRect(RenderFlags flags) {
        return (flags & 0x1) != 0;
    }

    bool renderFlagNoN(RenderFlags flags) {
        return ((flags >> 1) & 0x1) != 0;
    }

    bool renderFlagCulling(RenderFlags flags) {
        return ((flags >> 2) & 0x1) != 0;
    }

    bool renderFlagSmoothShade(RenderFlags flags) {
        return ((flags >> 3) & 0x1) != 0;
    }

    bool renderFlagLinearFiltering(RenderFlags flags) {
        return ((flags >> 4) & 0x1) != 0;
    }

    bool renderFlagSmoothNormal(RenderFlags flags) {
        return ((flags >> 5) & 0x1) != 0;
    }

    bool renderFlagNormalMap(RenderFlags flags) {
        return ((flags >> 6) & 0x1) != 0;
    }

    bool renderFlagShadowAlpha(RenderFlags flags) {
        return ((flags >> 7) & 0x1) != 0;
    }

    bool renderFlagOneCycleHardwareBug(RenderFlags flags) {
        return ((flags >> 8) & 0x1) != 0;
    }

    uint renderBlenderApproximation(RenderFlags flags) {
        return (flags >> 9) & 0x3;
    }

    bool renderFlagDynamicTiles(RenderFlags flags) {
        return ((flags >> 11) & 0x1) != 0;
    }

    bool renderFlagCanDecodeTMEM(RenderFlags flags) {
        return ((flags >> 12) & 0x1) != 0;
    }

    uint renderCMS0(RenderFlags flags) {
        return (flags >> 13) & 0x3;
    }

    uint renderCMT0(RenderFlags flags) {
        return (flags >> 15) & 0x3;
    }

    uint renderCMS1(RenderFlags flags) {
        return (flags >> 17) & 0x3;
    }

    uint renderCMT1(RenderFlags flags) {
        return (flags >> 19) & 0x3;
    }

    bool renderFlagUsesTexture0(RenderFlags flags) {
        return ((flags >> 21) & 0x1) != 0;
    }

    bool renderFlagUsesTexture1(RenderFlags flags) {
        return ((flags >> 22) & 0x1) != 0;
    }

    bool renderFlagUpscale2D(RenderFlags flags) {
        return ((flags >> 23) & 0x1) != 0;
    }

    bool renderFlagUpscaleLOD(RenderFlags flags) {
        return ((flags >> 24) & 0x1) != 0;
    }

    bool renderFlagUsesHDR(RenderFlags flags) {
        return ((flags >> 25) & 0x1) != 0;
    }
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