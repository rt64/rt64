//
// RT64
//

#pragma once

#include "rhi/rt64_render_interface.h"

namespace RT64 {
    struct Texture {
        uint64_t creationFrame = 0;
        std::unique_ptr<RenderTexture> texture;
        std::unique_ptr<RenderTexture> tmem;
        RenderFormat format = RenderFormat::UNKNOWN;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t mipmaps = 0;
        uint64_t memorySize = 0;

        // These are only stored if developer mode is enabled.
        std::vector<uint8_t> bytesTMEM;
    };
};