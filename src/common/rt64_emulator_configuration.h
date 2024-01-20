//
// RT64
//

#pragma once

#include "rt64_common.h"

namespace RT64 {
    struct EmulatorConfiguration {
        struct Framebuffer {
            bool renderToRAM;
            bool copyWithGPU;
        };

        Framebuffer framebuffer;

        EmulatorConfiguration();
    };
};