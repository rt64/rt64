//
// RT64
//

#pragma once

#include "common/rt64_load_types.h"
#include "common/rt64_plume.h"

namespace RT64 {
    struct Texture {
        uint64_t creationFrame = 0;
        std::unique_ptr<RenderTexture> texture;
        std::unique_ptr<RenderTexture> tmem;
        RenderFormat format = RenderFormat::UNKNOWN;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t tlut = 0;
        LoadTile loadTile;
        uint32_t mipmaps = 0;
        uint64_t memorySize = 0;
        std::vector<uint8_t> bytesTMEM;
        bool decodeTMEM = false;
    };
};
