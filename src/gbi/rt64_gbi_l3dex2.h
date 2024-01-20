//
// RT64
//

#pragma once

#include "rt64_gbi.h"

#define L3DEX2_G_LINE3D 0x08

namespace RT64 {
    namespace GBI_L3DEX2 {
        void line3D(State *state, DisplayList **dl);

        void setup(GBI *gbi);
    };
};