//
// RT64
//

#pragma once

#include "rt64_gbi.h"

#define F3DGOLDEN_G_MOVEWORD 0xBD
#define F3DGOLDEN_G_TRIX 0xB1

namespace RT64 {
    namespace GBI_F3DGOLDEN {
        void triX(State *state, DisplayList **dl);
        void setup(GBI *ucode);
    };
};