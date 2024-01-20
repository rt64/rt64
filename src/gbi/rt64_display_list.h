//
// RT64
//

#pragma once

namespace RT64 {
    struct DisplayList {
        uint32_t w0;
        uint32_t w1;

        DisplayList();
        uint32_t p0(uint8_t pos, uint8_t bits) const;
        uint32_t p1(uint8_t pos, uint8_t bits) const;
    };
};