//
// RT64
//

#pragma once

#include "rt64_common.h"

namespace RT64 {
    struct EnhancementConfiguration {
        struct Framebuffer {
            bool reinterpretFixULS;
        };

        struct Presentation {
            enum class Mode {
                Console,
                SkipBuffering,
                PresentEarly
            };

            Mode mode;
        };
        
        struct Rect {
            bool fixRectLR;
        };

        struct F3DEX {
            bool forceBranch;
        };

        struct S2DEX {
            bool fixBilerpMismatch;
            bool framebufferFastPath;
        };

        struct TextureLOD {
            bool scale;
        };

        Framebuffer framebuffer;
        Presentation presentation;
        Rect rect;
        F3DEX f3dex;
        S2DEX s2dex;
        TextureLOD textureLOD;

        EnhancementConfiguration();
    };
};