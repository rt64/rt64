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
            uint sampleCount : 2;
        };

        uint value;
    };
#else
    // SPIR-V code generation does not seem to like bitfields at the moment, so we work around it by querying the flags manually.
    bool renderFlagRect(uint flags) {
        return (flags & 0x1) != 0;
    }

    bool renderFlagNoN(uint flags) {
        return ((flags >> 1) & 0x1) != 0;
    }

    bool renderFlagCulling(uint flags) {
        return ((flags >> 2) & 0x1) != 0;
    }

    bool renderFlagSmoothShade(uint flags) {
        return ((flags >> 3) & 0x1) != 0;
    }

    bool renderFlagLinearFiltering(uint flags) {
        return ((flags >> 4) & 0x1) != 0;
    }

    uint renderBlenderApproximation(uint flags) {
        return (flags >> 5) & 0x3;
    }

    bool renderFlagDynamicTiles(uint flags) {
        return ((flags >> 7) & 0x1) != 0;
    }

    bool renderFlagCanDecodeTMEM(uint flags) {
        return ((flags >> 8) & 0x1) != 0;
    }

    uint renderCMS0(uint flags) {
        return (flags >> 9) & 0x3;
    }

    uint renderCMT0(uint flags) {
        return (flags >> 11) & 0x3;
    }

    uint renderCMS1(uint flags) {
        return (flags >> 13) & 0x3;
    }

    uint renderCMT1(uint flags) {
        return (flags >> 15) & 0x3;
    }

    bool renderFlagUsesTexture0(uint flags) {
        return ((flags >> 17) & 0x1) != 0;
    }

    bool renderFlagUsesTexture1(uint flags) {
        return ((flags >> 18) & 0x1) != 0;
    }

    uint renderFlagNativeSampler0(uint flags) {
        return (flags >> 19) & 0xF;
    }

    uint renderFlagNativeSampler1(uint flags) {
        return (flags >> 23) & 0xF;
    }

    bool renderFlagUpscale2D(uint flags) {
        return ((flags >> 27) & 0x1) != 0;
    }

    bool renderFlagUpscaleLOD(uint flags) {
        return ((flags >> 28) & 0x1) != 0;
    }

    bool renderFlagUsesHDR(uint flags) {
        return ((flags >> 29) & 0x1) != 0;
    }

    uint renderFlagSampleCount(uint flags) {
        return (flags >> 30) & 0x3;
    }
#endif
#ifdef HLSL_CPU
};
#endif