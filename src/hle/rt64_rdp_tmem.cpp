//
// RT64
//

#include "rt64_rdp_tmem.h"

#include <cassert>

#include "rt64_state.h"

namespace RT64 {
    // TextureManager

    uint64_t TextureManager::uploadTMEM(State *state, TextureCache *textureCache, uint64_t creationFrame, uint16_t byteOffset, uint16_t byteCount) {
        XXH3_state_t xxh3;
        XXH3_64bits_reset(&xxh3);
        const uint8_t *TMEM = reinterpret_cast<const uint8_t *>(state->rdp->TMEM);
        XXH3_64bits_update(&xxh3, &TMEM[byteOffset], byteCount);
        XXH3_64bits_update(&xxh3, &byteOffset, sizeof(byteOffset));
        XXH3_64bits_update(&xxh3, &byteCount, sizeof(byteCount));
        const uint64_t hash = XXH3_64bits_digest(&xxh3);
        if (hashSet.find(hash) == hashSet.end()) {
            hashSet.insert(hash);
            textureCache->queueGPUUploadTMEM(hash, creationFrame, TMEM, RDP_TMEM_BYTES, 0, 0, 0, LoadTile());
        }

        return hash;
    }

    uint64_t TextureManager::uploadTexture(State *state, const LoadTile &loadTile, TextureCache *textureCache, uint64_t creationFrame, uint16_t width, uint16_t height, uint32_t tlut) {
        XXH3_state_t xxh3;
        XXH3_64bits_reset(&xxh3);
        const bool RGBA32 = (loadTile.siz == G_IM_SIZ_32b) && (loadTile.fmt == G_IM_FMT_RGBA);
        const uint8_t *TMEM = reinterpret_cast<const uint8_t *>(state->rdp->TMEM);
        const uint32_t tmemSize = RGBA32 ? (RDP_TMEM_BYTES >> 1) : RDP_TMEM_BYTES;
        const uint32_t lastRowBytes = width << std::min(loadTile.siz, uint8_t(G_IM_SIZ_16b)) >> 1;
        const uint32_t bytesToHash = (loadTile.line << 3) * (height - 1) + lastRowBytes;
        const uint32_t tmemMask = RGBA32 ? RDP_TMEM_MASK16 : RDP_TMEM_MASK8;
        const uint32_t tmemAddress = (loadTile.tmem << 3) & tmemMask;
        auto hashTMEM = [&](uint32_t tmemOrAddress) {
            // Too many bytes to hash in a single step. Wrap around TMEM and hash the rest.
            if ((tmemAddress + bytesToHash) > tmemSize) {
                const uint32_t firstBytes = std::min(bytesToHash, std::max(tmemSize - tmemAddress, 0U));
                XXH3_64bits_update(&xxh3, &TMEM[tmemAddress | tmemOrAddress], firstBytes);
                XXH3_64bits_update(&xxh3, &TMEM[tmemOrAddress], std::min(bytesToHash - firstBytes, tmemAddress));
            }
            // Hash as normal.
            else {
                XXH3_64bits_update(&xxh3, &TMEM[tmemAddress | tmemOrAddress], bytesToHash);
            }
        };

        hashTMEM(0x0);

        if (RGBA32) {
            hashTMEM(tmemSize);
        }

        // If TLUT is active, we also hash the corresponding palette bytes.
        if (tlut > 0) {
            const bool CI4 = (loadTile.siz == G_IM_SIZ_4b);
            const int32_t paletteOffset = CI4 ? (loadTile.palette << 7) : 0;
            const int32_t bytesToHash = CI4 ? 0x80 : 0x800;
            const int32_t paletteAddress = (RDP_TMEM_BYTES >> 1) + paletteOffset;
            XXH3_64bits_update(&xxh3, &TMEM[paletteAddress], bytesToHash);
        }

        // Encode more parameters into the hash that affect the final RGBA32 output.
        XXH3_64bits_update(&xxh3, &width, sizeof(width));
        XXH3_64bits_update(&xxh3, &height, sizeof(height));
        XXH3_64bits_update(&xxh3, &tlut, sizeof(tlut));
        XXH3_64bits_update(&xxh3, &loadTile.line, sizeof(loadTile.line));
        XXH3_64bits_update(&xxh3, &loadTile.siz, sizeof(loadTile.siz));
        XXH3_64bits_update(&xxh3, &loadTile.fmt, sizeof(loadTile.fmt));

        const uint64_t hash = XXH3_64bits_digest(&xxh3);
        if (hashSet.find(hash) == hashSet.end()) {
            hashSet.insert(hash);
            textureCache->queueGPUUploadTMEM(hash, creationFrame, TMEM, RDP_TMEM_BYTES, width, height, tlut, loadTile);
        }

        return hash;
    }

    void TextureManager::removeHashes(const std::vector<uint64_t> &hashes) {
        for (uint64_t hash : hashes) {
            hashSet.erase(hash);
        }
    }

    bool TextureManager::requiresRawTMEM(const LoadTile &loadTile, uint16_t width, uint16_t height) {
        const bool RGBA32 = (loadTile.siz == G_IM_SIZ_32b) && (loadTile.fmt == G_IM_FMT_RGBA);
        const uint32_t tmemSize = RGBA32 ? (RDP_TMEM_BYTES >> 1) : RDP_TMEM_BYTES;
        const uint32_t lastRowBytes = width << std::min(loadTile.siz, uint8_t(G_IM_SIZ_16b)) >> 1;
        const uint32_t bytesToHash = (loadTile.line << 3) * (height - 1) + lastRowBytes;
        return (bytesToHash > tmemSize);
    }
};