//
// RT64
//

#include "rt64_rdp_tmem.h"

#include <cassert>
#include <cinttypes>

#include "xxHash/xxh3.h"

#include "common/rt64_tmem_hasher.h"

#include "rt64_state.h"

namespace RT64 {
    // TextureManager

    void TextureManager::uploadEmpty(State *state, TextureCache *textureCache, uint64_t creationFrame, uint16_t width, uint16_t height, uint64_t replacementHash) {
        if (hashSet.find(replacementHash) == hashSet.end()) {
            hashSet.insert(replacementHash);
            textureCache->queueGPUUploadTMEM(replacementHash, creationFrame, nullptr, 0, width, height, 0, LoadTile(), false);
        }
    }

    uint64_t TextureManager::uploadTMEM(State *state, const LoadTile &loadTile, TextureCache *textureCache, uint64_t creationFrame, uint16_t byteOffset, uint16_t byteCount, uint16_t width, uint16_t height, uint32_t tlut) {
        XXH3_state_t xxh3;
        XXH3_64bits_reset(&xxh3);
        const uint8_t *TMEM = reinterpret_cast<const uint8_t *>(state->rdp->TMEM);
        XXH3_64bits_update(&xxh3, &TMEM[byteOffset], byteCount);
        XXH3_64bits_update(&xxh3, &byteOffset, sizeof(byteOffset));
        XXH3_64bits_update(&xxh3, &byteCount, sizeof(byteCount));
        const uint64_t hash = XXH3_64bits_digest(&xxh3);
        if (hashSet.find(hash) == hashSet.end()) {
            hashSet.insert(hash);
            textureCache->queueGPUUploadTMEM(hash, creationFrame, TMEM, RDP_TMEM_BYTES, width, height, 0, LoadTile(), false);
        }
        
        // Dump memory contents into a file if the process is active.
        if (!state->dumpingTexturesDirectory.empty()) {
            // Since width and height are not exactly guaranteed to be sane values when using raw TMEM, ensure we only dump textures when the values make sense.
            const bool validTextureCheck = (width > 0x0) && (height > 0x0);
            const bool bigTextureCheck = (width > 0x1000) || (height > 0x1000);
            if (validTextureCheck && !bigTextureCheck) {
                dumpTexture(hash, state, loadTile, width, height, tlut);
            }
        }

        return hash;
    }

    uint64_t TextureManager::uploadTexture(State *state, const LoadTile &loadTile, TextureCache *textureCache, uint64_t creationFrame, uint16_t width, uint16_t height, uint32_t tlut) {
        const uint8_t *TMEM = reinterpret_cast<const uint8_t *>(state->rdp->TMEM);
        uint64_t hash = TMEMHasher::hash(TMEM, loadTile, width, height, tlut, TMEMHasher::CurrentHashVersion);
        if (hashSet.find(hash) == hashSet.end()) {
            hashSet.insert(hash);
            textureCache->queueGPUUploadTMEM(hash, creationFrame, TMEM, RDP_TMEM_BYTES, width, height, tlut, loadTile, true);
        }

        // Dump memory contents into a file if the process is active.
        if (!state->dumpingTexturesDirectory.empty()) {
            dumpTexture(hash, state, loadTile, width, height, tlut);
        }

        return hash;
    }

    void TextureManager::dumpTexture(uint64_t hash, State *state, const LoadTile &loadTile, uint16_t width, uint16_t height, uint32_t tlut) {
        if (dumpedSet.find(hash) != dumpedSet.end()) {
            return;
        }

        // Insert into set regardless of whether the dump is successful or not.
        dumpedSet.insert(hash);
        
        // Dump the entirety of TMEM.
        char baseName[64];
        snprintf(baseName, sizeof(baseName), "%016" PRIx64 ".v%u", hash, TMEMHasher::CurrentHashVersion);
        std::filesystem::path dumpTmemPath = state->dumpingTexturesDirectory / (std::string(baseName) + ".tmem");
        std::ofstream dumpTmemStream(dumpTmemPath, std::ios::binary);
        if (dumpTmemStream.is_open()) {
            const char *TMEM = reinterpret_cast<const char *>(state->rdp->TMEM);
            dumpTmemStream.write(TMEM, RDP_TMEM_BYTES);
            dumpTmemStream.close();
        }

        // Dump the RDRAM last loaded into the TMEM address pointed to by the tile. Required for generating hashes used by Rice.
        const LoadOperation &loadOp = state->rdp->rice.lastLoadOpByTMEM[loadTile.tmem];
        uint32_t rdramStart = loadOp.texture.address;
        uint32_t rdramCount = 0;
        uint32_t commonBytesOffset = (loadOp.tile.uls >> 2) << loadOp.texture.siz >> 1;
        uint32_t commonBytesPerRow = loadOp.texture.width << loadOp.texture.siz >> 1;
        if (loadOp.type == LoadOperation::Type::Block) {
            uint32_t wordCount = ((loadOp.tile.lrs - loadOp.tile.uls) >> (4 - loadOp.tile.siz)) + 1;
            rdramStart = loadOp.texture.address + commonBytesOffset + commonBytesPerRow * loadOp.tile.ult;
            rdramCount = (wordCount << 3);

            // Increase the amount of RDRAM dumped by textures that require padding when using load block.
            commonBytesPerRow = std::max(commonBytesPerRow, uint32_t(loadTile.line) << 3U);
        }
        else if (loadOp.type == LoadOperation::Type::Tile) {
            uint32_t rowCount = 1 + ((loadOp.tile.lrt >> 2) - (loadOp.tile.ult >> 2));
            uint32_t tileWidth = ((loadOp.tile.lrs >> 2) - (loadOp.tile.uls >> 2));
            uint32_t wordsPerRow = (tileWidth >> (4 - loadOp.tile.siz)) + 1;
            rdramStart = loadOp.texture.address + commonBytesOffset + commonBytesPerRow * (loadOp.tile.ult >> 2);
            rdramCount = rowCount * commonBytesPerRow;
        }
        
        // Dump more RDRAM if necessary if it doesn't cover what the tile could possibly sample.
        uint32_t loadTileBpr = width << loadTile.siz >> 1;
        rdramCount = std::max(rdramCount, std::max(loadTileBpr, commonBytesPerRow) * height);

        if (rdramCount > 0) {
            std::filesystem::path dumpRdramPath = state->dumpingTexturesDirectory / (std::string(baseName) + ".rice.rdram");
            std::ofstream dumpRdramStream(dumpRdramPath, std::ios::binary);
            if (dumpRdramStream.is_open()) {
                const char *RDRAM = reinterpret_cast<const char *>(state->RDRAM);
                dumpRdramStream.write(&RDRAM[rdramStart], rdramCount);
                dumpRdramStream.close();
            }

            std::filesystem::path dumpRdramInfoPath = state->dumpingTexturesDirectory / (std::string(baseName) + ".rice.json");
            std::ofstream dumpRdramInfoStream(dumpRdramInfoPath);
            if (dumpRdramInfoStream.is_open()) {
                json jroot;
                jroot["tile"] = loadOp.tile;
                jroot["type"] = loadOp.type;
                jroot["texture"] = loadOp.texture;
                dumpRdramInfoStream << std::setw(4) << jroot << std::endl;
                dumpRdramInfoStream.close();
            }
        }
        
        // Repeat a similar process for dumping the palette.
        if (tlut > 0) {
            const bool CI4 = (loadTile.siz == G_IM_SIZ_4b);
            const int32_t paletteTMEM = (RDP_TMEM_WORDS >> 1) + (CI4 ? (loadTile.palette << 4) : 0);
            const LoadOperation &paletteLoadOp = state->rdp->rice.lastLoadOpByTMEM[paletteTMEM];
            uint32_t paletteBytesOffset = (paletteLoadOp.tile.uls >> 2) << paletteLoadOp.texture.siz >> 1;
            uint32_t paletteBytesPerRow = paletteLoadOp.texture.width << paletteLoadOp.texture.siz >> 1;
            const uint32_t rowCount = 1 + ((paletteLoadOp.tile.lrt >> 2) - (paletteLoadOp.tile.ult >> 2));
            const uint32_t wordsPerRow = ((paletteLoadOp.tile.lrs >> 2) - (paletteLoadOp.tile.uls >> 2)) + 1;
            uint32_t paletteRdramStart = paletteLoadOp.texture.address + paletteBytesOffset + paletteBytesPerRow * (paletteLoadOp.tile.ult >> 2);
            uint32_t paletteRdramCount = (rowCount - 1) * paletteBytesPerRow + (wordsPerRow << 3);
            if (paletteRdramCount > 0) {
                std::filesystem::path dumpPaletteRdramPath = state->dumpingTexturesDirectory / (std::string(baseName) + ".rice.palette.rdram");
                std::ofstream dumpPaletteRdramStream(dumpPaletteRdramPath, std::ios::binary);
                if (dumpPaletteRdramStream.is_open()) {
                    const char *RDRAM = reinterpret_cast<const char *>(state->RDRAM);
                    dumpPaletteRdramStream.write(&RDRAM[paletteRdramStart], paletteRdramCount);
                    dumpPaletteRdramStream.close();
                }
            }

            std::filesystem::path dumpPaletteRdramInfoPath = state->dumpingTexturesDirectory / (std::string(baseName) + ".rice.palette.json");
            std::ofstream dumpPaletteRdramInfoStream(dumpPaletteRdramInfoPath);
            if (dumpPaletteRdramInfoStream.is_open()) {
                json jroot;
                jroot["tile"] = paletteLoadOp.tile;
                jroot["type"] = paletteLoadOp.type;
                jroot["texture"] = paletteLoadOp.texture;
                dumpPaletteRdramInfoStream << std::setw(4) << jroot << std::endl;
                dumpPaletteRdramInfoStream.close();
            }
        }

        // Dump the parameters of the tile into a JSON file.
        std::filesystem::path dumpTilePath = state->dumpingTexturesDirectory / (std::string(baseName) + ".tile.json");
        std::ofstream dumpTileStream(dumpTilePath);
        if (dumpTileStream.is_open()) {
            json jroot;
            jroot["tile"] = loadTile;
            jroot["width"] = width;
            jroot["height"] = height;

            // Serialize the TLUT into an enum instead.
            if (tlut == G_TT_RGBA16) {
                jroot["tlut"] = LoadTLUT::RGBA16;
            }
            else if (tlut == G_TT_IA16) {
                jroot["tlut"] = LoadTLUT::IA16;
            }
            else {
                jroot["tlut"] = LoadTLUT::None;
            }

            dumpTileStream << std::setw(4) << jroot << std::endl;
            dumpTileStream.close();
        }
    }

    void TextureManager::removeHashes(const std::vector<uint64_t> &hashes) {
        for (uint64_t hash : hashes) {
            hashSet.erase(hash);
        }
    }
};