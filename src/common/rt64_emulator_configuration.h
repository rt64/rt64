//
// RT64
//

#pragma once

#include "rt64_common.h"

namespace RT64 {
    struct EmulatorConfiguration {
        struct Dither {
            bool postBlendNoise;
            bool postBlendNoiseNegative;
        };

        struct Framebuffer {
            bool renderToRAM;
            bool copyWithGPU;
        };

        Dither dither;
        Framebuffer framebuffer;

        EmulatorConfiguration();
    };
};