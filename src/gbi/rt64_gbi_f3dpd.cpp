//
// RT64
//

#include "rt64_gbi_f3dpd.h"

#include "hle/rt64_interpreter.h"

#include "rt64_gbi_f3d.h"
#include "rt64_gbi_f3dgolden.h"

namespace RT64 {
    namespace GBI_F3DPD {
        void vertex(State *state, DisplayList **dl) {
            state->rsp->setVertexPD((*dl)->w1, (*dl)->p0(20, 4) + 1, (*dl)->p0(16, 4));
        }

        void vertexColor(State *state, DisplayList **dl) {
            state->rsp->setVertexColorPD((*dl)->w1);
        }

        void setup(GBI *gbi) {
            GBI_F3D::setup(gbi);

            gbi->map[F3D_G_VTX] = &vertex;
            gbi->map[F3DPD_G_VTXCOLOR] = &vertexColor;
            gbi->map[F3DGOLDEN_G_TRIX] = &GBI_F3DGOLDEN::triX;
        }
    }
};