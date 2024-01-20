//
// RT64
//

#include "rt64_gbi_f3dgolden.h"

#include "hle/rt64_interpreter.h"

#include "rt64_gbi_f3d.h"

namespace RT64 {
    namespace GBI_F3DGOLDEN {
        void triX(State *state, DisplayList **dl) {
            uint32_t w0 = (*dl)->w0;
            uint32_t w1 = (*dl)->w1;
            while (w1 != 0) {
                uint32_t v0 = w1 & 0xf;
                w1 >>= 4;

                uint32_t v1 = w1 & 0xf;
                w1 >>= 4;

                uint32_t v2 = w0 & 0xf;
                w0 >>= 4;

                state->rsp->drawIndexedTri(v0, v1, v2);
            }
        }
        
        void setup(GBI *gbi) {
            GBI_F3D::setup(gbi);

            gbi->map[F3DGOLDEN_G_TRIX] = &triX;
            gbi->map[F3DGOLDEN_G_MOVEWORD] = GBI_F3D::moveWord;
        }
    }
};