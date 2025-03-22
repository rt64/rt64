//
// RT64
//

#pragma once

#include "common/rt64_common.h"

namespace RT64 {
    struct ColorConverter {
        struct RGBA16 {
            static uint16_t toRGBA(hlslpp::float4 src);
            static hlslpp::float4 toRGBAF(uint16_t src);
        };

        struct RGBA32 {
            static uint32_t toRGBA(hlslpp::float4 src);
            static hlslpp::float4 toRGBAF(uint32_t src);
        };

        struct D16 {
            static float toF(uint16_t src);
        };
    };
};