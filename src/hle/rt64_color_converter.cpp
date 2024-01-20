//
// RT64
//

#include "rt64_color_converter.h"

#include <algorithm>

#include "common/rt64_math.h"

namespace RT64 {
    // ColorConverter::RGBA16

    hlslpp::float4 ColorConverter::RGBA16::toRGBAF(uint16_t src) {
        uint8_t r = (src >> 11) & 0x1F;
        uint8_t g = (src >> 6)  & 0x1F;
        uint8_t b = (src >> 1)  & 0x1F;
        return {
            ((r << 3) | (r >> 2)) / 255.0f,
            ((g << 3) | (g >> 2)) / 255.0f,
            ((b << 3) | (b >> 2)) / 255.0f,
            (src & 1) ? 1.0f : 0.0f
        };
    }

    // ColorConverter::RGBA32

    uint32_t ColorConverter::RGBA32::toRGBA(hlslpp::float4 src) {
        hlslpp::uint4 srcUint = hlslpp::uint4(hlslpp::round(hlslpp::clamp(src * 255.0f, 0.0f, 255.0f)));
        return (srcUint[0] << 24) | (srcUint[1] << 16) | (srcUint[2] << 8) | (srcUint[3] << 0);
    }

    hlslpp::float4 ColorConverter::RGBA32::toRGBAF(uint32_t src) {
        return {
            ((src >> 24) & 0xFF) / 255.0f,
            ((src >> 16) & 0xFF) / 255.0f,
            ((src >> 8)  & 0xFF) / 255.0f,
            ((src >> 0)  & 0xFF) / 255.0f
        };
    }
};