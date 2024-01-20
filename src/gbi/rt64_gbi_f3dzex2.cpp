//
// RT64
//

#include "rt64_gbi_f3dzex2.h"

#include "rt64_gbi_f3dex2.h"

namespace RT64 {
    namespace GBI_F3DZEX2 {
        void branchW(State *state, DisplayList **dl) {
            state->rsp->branchW(state->microcode.half1, (*dl)->p0(1, 7), (*dl)->w1, dl);
        }

        void setup(GBI *gbi) {
            GBI_F3DEX2::setup(gbi);

            gbi->map[F3DZEX2_G_BRANCH_W] = &branchW;
        }
    }
};