//
// RT64
//

#pragma once

#include "shared/rt64_hlsl.h"

#define NATIVE_SAMPLER_NONE           0
#define NATIVE_SAMPLER_WRAP_WRAP      1
#define NATIVE_SAMPLER_WRAP_MIRROR    2
#define NATIVE_SAMPLER_WRAP_CLAMP     3
#define NATIVE_SAMPLER_MIRROR_WRAP    4
#define NATIVE_SAMPLER_MIRROR_MIRROR  5
#define NATIVE_SAMPLER_MIRROR_CLAMP   6
#define NATIVE_SAMPLER_CLAMP_WRAP     7
#define NATIVE_SAMPLER_CLAMP_MIRROR   8
#define NATIVE_SAMPLER_CLAMP_CLAMP    9

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
            uint blenderApproximation : 2;
            uint dynamicTiles : 1;
            uint canDecodeTMEM : 1;
            uint cms0 : 2;
            uint cmt0 : 2;
            uint cms1 : 2;
            uint cmt1 : 2;
            uint usesTexture0 : 1;
            uint usesTexture1 : 1;
            uint nativeSampler0 : 4;
            uint nativeSampler1 : 4;
            uint upscale2D : 1;
            uint upscaleLOD : 1;
            uint usesHDR : 1;
            uint smoothNormal : 1;
            uint shadowAlpha : 1;
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

    uint renderBlenderApproximation(RenderFlags flags) {
        return (flags >> 5) & 0x3;
    }

    bool renderFlagDynamicTiles(RenderFlags flags) {
        return ((flags >> 7) & 0x1) != 0;
    }

    bool renderFlagCanDecodeTMEM(RenderFlags flags) {
        return ((flags >> 8) & 0x1) != 0;
    }

    uint renderCMS0(RenderFlags flags) {
        return (flags >> 9) & 0x3;
    }

    uint renderCMT0(RenderFlags flags) {
        return (flags >> 11) & 0x3;
    }

    uint renderCMS1(RenderFlags flags) {
        return (flags >> 13) & 0x3;
    }

    uint renderCMT1(RenderFlags flags) {
        return (flags >> 15) & 0x3;
    }

    bool renderFlagUsesTexture0(RenderFlags flags) {
        return ((flags >> 17) & 0x1) != 0;
    }

    bool renderFlagUsesTexture1(RenderFlags flags) {
        return ((flags >> 18) & 0x1) != 0;
    }

    uint renderFlagNativeSampler0(RenderFlags flags) {
        return (flags >> 19) & 0xF;
    }

    uint renderFlagNativeSampler1(RenderFlags flags) {
        return (flags >> 23) & 0xF;
    }

    bool renderFlagUpscale2D(RenderFlags flags) {
        return ((flags >> 27) & 0x1) != 0;
    }

    bool renderFlagUpscaleLOD(RenderFlags flags) {
        return ((flags >> 28) & 0x1) != 0;
    }

    bool renderFlagUsesHDR(RenderFlags flags) {
        return ((flags >> 29) & 0x1) != 0;
    }

    bool renderFlagSmoothNormal(RenderFlags flags) {
        return ((flags >> 30) & 0x1) != 0;
    }

    bool renderFlagShadowAlpha(RenderFlags flags) {
        return ((flags >> 31) & 0x1) != 0;
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