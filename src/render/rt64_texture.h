//
// RT64
//

#pragma once

#include "rhi/rt64_render_interface.h"

namespace RT64 {
    struct Texture {
        uint64_t hash = 0;
        uint64_t creationFrame = 0;
        std::unique_ptr<RenderTexture> texture;
        std::unique_ptr<RenderTexture> tmem;
        RenderFormat format = RenderFormat::UNKNOWN;
        int width = 0;
        int height = 0;

        // These are only stored if developer mode is enabled.
        std::vector<uint8_t> bytesTMEM;
    };
};