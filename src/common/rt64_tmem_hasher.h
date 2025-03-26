//
// RT64
//

#pragma once

namespace RT64 {
    struct TMEMHasher {
        static const uint32_t CurrentHashVersion = 4;

        static bool needsToHashRowsIndividually(const LoadTile &loadTile, uint32_t width) {
            // When using 32-bit formats, TMEM contents are split in half in the lower and upper half, so the size per row is effectively
            // the same as a 16-bit format as far as TMEM is concerned.
            const bool RGBA32 = (loadTile.siz == G_IM_SIZ_32b) && (loadTile.fmt == G_IM_FMT_RGBA);
            uint32_t drawBytesPerRow = std::max(width << (RGBA32 ? G_IM_SIZ_16b : loadTile.siz) >> 1U, 1U);
            uint32_t tmemBytesPerRow = loadTile.line << 3;
            return tmemBytesPerRow > drawBytesPerRow;
        }

        static uint64_t hash(const uint8_t *TMEM, const LoadTile &loadTile, uint16_t width, uint16_t height, uint32_t tlut, uint32_t version) {
            const uint32_t TMEMBytes = 4096;
            const uint32_t TMEMMask8 = 4095;
            const uint32_t TMEMMask16 = 2047;
            XXH3_state_t xxh3;
            XXH3_64bits_reset(&xxh3);
            const bool RGBA32 = (loadTile.siz == G_IM_SIZ_32b) && (loadTile.fmt == G_IM_FMT_RGBA);
            const bool usesTLUT = tlut > 0;
            bool halfTMEM = RGBA32;
            
            // Version 3 fixes an error where using TLUT did not mask the address to the lower half of TMEM.
            if ((version >= 3) && usesTLUT) {
                halfTMEM = true;
            }

            const uint32_t tmemSize = halfTMEM ? (TMEMBytes >> 1) : TMEMBytes;
            const uint32_t drawBytesPerRow = std::max(uint32_t(width) << (RGBA32 ? G_IM_SIZ_16b : loadTile.siz) >> 1U, 1U);
            const uint32_t drawBytesTotal = (loadTile.line << 3) * (height - 1) + drawBytesPerRow;
            const uint32_t tmemMask = halfTMEM ? TMEMMask16 : TMEMMask8;
            const uint32_t tmemAddress = (loadTile.tmem << 3) & tmemMask;
            auto hashTMEM = [&](uint32_t tmemBaseAddress, uint32_t tmemOrAddress, uint32_t byteCount, bool oddRow) {
                assert((tmemBaseAddress < tmemSize) && "Base address must be masked within the bounds of the TMEM size.");

                // Too many bytes to hash in a single step. Wrap around TMEM and hash the rest.
                if ((tmemBaseAddress + byteCount) > tmemSize) {
                    const uint32_t firstBytes = (tmemSize - tmemBaseAddress);
                    XXH3_64bits_update(&xxh3, &TMEM[tmemBaseAddress | tmemOrAddress], firstBytes);

                    // Start hashing from the start of TMEM.
                    byteCount = std::min(byteCount - firstBytes, tmemBaseAddress);
                    tmemBaseAddress = 0;
                }

                // Version 4 fixes an error where odd row word swapping was not considered when hashing rows individually.
                if (oddRow && (version >= 4)) {
                    uint32_t wordCount = byteCount / 8;
                    if (wordCount > 0) {
                        XXH3_64bits_update(&xxh3, &TMEM[tmemBaseAddress | tmemOrAddress], wordCount * 8);
                        tmemBaseAddress += wordCount * 8;
                        byteCount -= wordCount * 8;
                    }
                    
                    if (byteCount > 4) {
                        XXH3_64bits_update(&xxh3, &TMEM[tmemBaseAddress | tmemOrAddress], byteCount - 4);
                        XXH3_64bits_update(&xxh3, &TMEM[(tmemBaseAddress + 4) | tmemOrAddress], 4);
                    }
                    else if (byteCount > 0) {
                        XXH3_64bits_update(&xxh3, &TMEM[(tmemBaseAddress + 4) | tmemOrAddress], byteCount);
                    }
                }
                else {
                    XXH3_64bits_update(&xxh3, &TMEM[tmemBaseAddress | tmemOrAddress], byteCount);
                }
            };

            // Version 2 introduces the ability to hash row by row because not all bytes of the line in TMEM are used.
            if ((version >= 2) && TMEMHasher::needsToHashRowsIndividually(loadTile, width)) {
                uint32_t tmemBytesPerRow = loadTile.line << 3;
                for (uint32_t i = 0; i < height; i++) {
                    hashTMEM((tmemAddress + i * tmemBytesPerRow) & tmemMask, 0x0, drawBytesPerRow, i & 1);
                }

                if (RGBA32) {
                    for (uint32_t i = 0; i < height; i++) {
                        hashTMEM((tmemAddress + i * tmemBytesPerRow) & tmemMask, tmemSize, drawBytesPerRow, i & 1);
                    }
                }
            }
            else {
                hashTMEM(tmemAddress, 0x0, drawBytesTotal, false);

                if (RGBA32) {
                    hashTMEM(tmemAddress, tmemSize, drawBytesTotal, false);
                }
            }

            // If TLUT is active, we also hash the corresponding palette bytes.
            if (usesTLUT) {
                const bool CI4 = (loadTile.siz == G_IM_SIZ_4b);
                const int32_t paletteOffset = CI4 ? (loadTile.palette << 7) : 0;
                const int32_t bytesToHash = CI4 ? 0x80 : 0x800;
                const int32_t paletteAddress = (TMEMBytes >> 1) + paletteOffset;
                XXH3_64bits_update(&xxh3, &TMEM[paletteAddress], bytesToHash);
            }
            
            // Encode more parameters into the hash that affect the final RGBA32 output.
            static_assert(sizeof(width) == 2, "Hash must use 16-bit width.");
            static_assert(sizeof(height) == 2, "Hash must use 16-bit height.");
            static_assert(sizeof(tlut) == 4, "Hash must use 32-bit TLUT.");
            static_assert(sizeof(loadTile.line) == 2, "Hash must use 16-bit line.");
            static_assert(sizeof(loadTile.siz) == 1, "Hash must use 8-bit siz.");
            static_assert(sizeof(loadTile.fmt) == 1, "Hash must use 8-bit fmt.");
            XXH3_64bits_update(&xxh3, &width, sizeof(width));
            XXH3_64bits_update(&xxh3, &height, sizeof(height));
            XXH3_64bits_update(&xxh3, &tlut, sizeof(tlut));
            XXH3_64bits_update(&xxh3, &loadTile.line, sizeof(loadTile.line));
            XXH3_64bits_update(&xxh3, &loadTile.siz, sizeof(loadTile.siz));
            XXH3_64bits_update(&xxh3, &loadTile.fmt, sizeof(loadTile.fmt));

            return XXH3_64bits_digest(&xxh3);
        }

        static bool requiresRawTMEM(const LoadTile &loadTile, uint16_t width, uint16_t height) {
            const uint32_t TMEMBytes = 4096;
            const bool RGBA32 = (loadTile.siz == G_IM_SIZ_32b) && (loadTile.fmt == G_IM_FMT_RGBA);
            const uint32_t tmemSize = RGBA32 ? (TMEMBytes >> 1) : TMEMBytes;
            const uint32_t lastRowBytes = width << std::min(loadTile.siz, uint8_t(G_IM_SIZ_16b)) >> 1;
            const uint32_t bytesToHash = (loadTile.line << 3) * (height - 1) + lastRowBytes;
            return (bytesToHash > tmemSize);
        }
    };
};