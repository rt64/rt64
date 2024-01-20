//
// RT64
//

#pragma once

#include "rt64_gbi.h"

#define F3DZEX2_G_BRANCH_W 0x04

namespace RT64 {
    namespace GBI_F3DZEX2 {
        void branchW(State *state, DisplayList **dl);

        void setup(GBI *gbi);
    };
};