//
// RT64
//

#pragma once

#include <set>

#include "hle/rt64_draw_call.h"
#include "render/rt64_texture_cache.h"

namespace RT64 {
    struct State;

    struct TextureManager {
        std::set<uint64_t> hashSet;

        uint64_t uploadTMEM(State *state, TextureCache *textureCache, uint64_t creationFrame, uint16_t byteOffset, uint16_t byteCount);
        uint64_t uploadTexture(State *state, const LoadTile &loadTile, TextureCache *textureCache, uint64_t creationFrame, uint16_t width, uint16_t height, uint32_t tlut);
        void removeHashes(const std::vector<uint64_t> &hashes);
        static bool requiresRawTMEM(const LoadTile &loadTile, uint16_t width, uint16_t height);
    };
};