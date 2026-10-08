//
// RT64
//

#pragma once

#include <cstdint>
#include <vector>

#include "rt64_load_types.h"

#include "shared/rt64_texture_decoder.h"

namespace RT64 {
    struct TMEMDecoder {
        static void decodeToRGBA32(const uint8_t *TMEM, const LoadTile &loadTile, uint32_t width, uint32_t height, uint32_t tlutFormat, std::vector<uint32_t> &pixels) {
            const interop::uint address = loadTile.tmem << 3;
            const interop::uint stride = loadTile.line << 3;
            pixels.resize(width * height);

            interop::float4 c;
            for (uint32_t y = 0; y < height; y++) {
                for (uint32_t x = 0; x < width; x++) {
                    c = interop::sampleTMEM(interop::int2(x, y), loadTile.siz, loadTile.fmt, address, stride, tlutFormat, loadTile.palette, TMEM);
                    pixels[y * width + x] = (interop::FloatToUINT8(c.r) << 0) | (interop::FloatToUINT8(c.g) << 8) | (interop::FloatToUINT8(c.b) << 16) | (interop::FloatToUINT8(c.a) << 24);
                }
            }
        }
    };
};