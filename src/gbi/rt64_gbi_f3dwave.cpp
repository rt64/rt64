//
// RT64
//

#include "rt64_gbi_f3dwave.h"

#include "hle/rt64_interpreter.h"

#include "rt64_gbi_f3d.h"

namespace RT64 {
    namespace GBI_F3DWAVE {
        void vertex(State *state, DisplayList **dl) {
            state->rsp->setVertex((*dl)->w1, (*dl)->p0(9, 7), (*dl)->p0(16, 8) / 5);
        }

        void tri1(State *state, DisplayList **dl) {
            state->rsp->drawIndexedTri((*dl)->p1(16, 8) / 5, (*dl)->p1(8, 8) / 5, (*dl)->p1(0, 8) / 5);
        }

        void tri2(State *state, DisplayList **dl) {
            state->rsp->drawIndexedTri((*dl)->p0(16, 8) / 5, (*dl)->p0(8, 8) / 5, (*dl)->p0(0, 8) / 5);
            state->rsp->drawIndexedTri((*dl)->p1(16, 8) / 5, (*dl)->p1(8, 8) / 5, (*dl)->p1(0, 8) / 5);
        }

        void quad(State *state, DisplayList **dl) {
            const uint8_t v0 = (*dl)->p1(24, 8) / 5;
            const uint8_t v1 = (*dl)->p1(16, 8) / 5;
            const uint8_t v2 = (*dl)->p1(8, 8) / 5;
            const uint8_t v3 = (*dl)->p1(0, 8) / 5;
            state->rsp->drawIndexedTri(v0, v1, v2);
            state->rsp->drawIndexedTri(v0, v2, v3);
        }

        void setup(GBI *gbi) {
            GBI_F3D::setup(gbi);

            gbi->map[F3DWAVE_G_UNKNOWN] = nullptr; // FIXME: Replaces a function set by base F3D with nothing until it's figured out.
            gbi->map[F3DWAVE_G_RDPHALF_1] = &GBI_F3D::rdpHalf1;
            gbi->map[F3DWAVE_G_RDPHALF_2] = &GBI_F3D::rdpHalf2;
            gbi->map[F3D_G_VTX] = vertex;
            gbi->map[F3D_G_TRI1] = tri1;
            gbi->map[F3DWAVE_G_TRI2] = &tri2;
            gbi->map[F3D_G_QUAD] = quad;
        }
    }
};