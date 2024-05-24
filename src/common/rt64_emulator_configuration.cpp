//
// RT64
//

#include "rt64_emulator_configuration.h"

namespace RT64 {
    // EmulatorConfiguration

    EmulatorConfiguration::EmulatorConfiguration() {
        dither.postBlendNoise = true;
        framebuffer.renderToRAM = true;
        framebuffer.copyWithGPU = true;
    }
};