//
// RT64
//

#pragma once

#include "rt64_gbi.h"

#define F3DEX_G_LOAD_UCODE 0xAF
#define F3DEX_G_MODIFYVTX 0xB2
#define F3DEX_G_BRANCH_Z 0xB0
#define F3DEX_G_TRI2 0xB1

namespace RT64 {
    namespace GBI_F3DEX {
        void vertex(State *state, DisplayList **dl);
        void modifyVertex(State *state, DisplayList **dl);
        void tri1(State *state, DisplayList **dl);
        void tri2(State *state, DisplayList **dl);
        void quad(State *state, DisplayList **dl);
        void cullDl(State *state, DisplayList **dl);
        void branchZ(State *state, DisplayList **dl);
        void loadUCode(State *state, DisplayList **dl);

        void setup(GBI *ucode);
    };
};