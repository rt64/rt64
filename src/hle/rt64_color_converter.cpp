//
// RT64
//

#include "rt64_color_converter.h"

#include <algorithm>

#include "common/rt64_math.h"

#define DEPTH_EXPONENT_MASK     0xE000
#define DEPTH_MANTISSA_MASK     0x1FFC
#define DEPTH_EXPONENT_SHIFT    13
#define DEPTH_MANTISSA_SHIFT    2

namespace RT64 {
    // ColorConverter::RGBA16

    uint16_t ColorConverter::RGBA16::toRGBA(hlslpp::float4 src) {
        uint16_t r = uint16_t(hlslpp::round(hlslpp::clamp(src.r * 255.0f, 0.0f, 255.0f)));
        uint16_t g = uint16_t(hlslpp::round(hlslpp::clamp(src.g * 255.0f, 0.0f, 255.0f)));
        uint16_t b = uint16_t(hlslpp::round(hlslpp::clamp(src.b * 255.0f, 0.0f, 255.0f)));
        uint16_t a = uint16_t(hlslpp::round(hlslpp::clamp(src.a * 255.0f, 0.0f, 255.0f)));
        r = std::min(r, uint16_t(255)) >> 3;
        g = std::min(g, uint16_t(255)) >> 3;
        b = std::min(b, uint16_t(255)) >> 3;
        a = (a > 0) ? 1 : 0;
        return (r << 11) | (g << 6) | (b << 1) | a;
    }

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

    // ColorConverter::D16

    float ColorConverter::D16::toF(uint16_t src) {
        // Extract the exponent and mantissa from the depth buffer value.
        uint32_t exponent = (src & DEPTH_EXPONENT_MASK) >> DEPTH_EXPONENT_SHIFT;
        uint32_t mantissa = (src & DEPTH_MANTISSA_MASK) >> DEPTH_MANTISSA_SHIFT;

        // Convert the exponent and mantissa into a fixed-point value.
        uint32_t shiftedMantissa = mantissa << (6 - std::min(6U, exponent));
        uint32_t mantissaBias = 0x40000U - (0x40000U >> exponent);
        return (shiftedMantissa + mantissaBias) / (32768.0f * 8.0f - 1);
    }
};