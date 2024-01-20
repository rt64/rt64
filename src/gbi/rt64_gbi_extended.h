//
// RT64
//

#pragma once

#include "rt64_gbi.h"

#define F3DZEX2_G_BRANCH_W 0x04

namespace RT64 {
    namespace GBI_EXTENDED {
        void initialize();
        void noOpHook(State *state, DisplayList **dl);
        void extendedOp(State *state, DisplayList **dl);
    };
};