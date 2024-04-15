//
// RT64
//

#include "rt64_gbi_s2dex2.h"

#include "rt64_gbi_extended.h"
#include "rt64_gbi_f3d.h"
#include "rt64_gbi_f3dex.h"
#include "rt64_gbi_f3dex2.h"
#include "rt64_gbi_rdp.h"
#include "rt64_gbi_s2dex.h"

namespace RT64 {
    namespace GBI_S2DEX2 {
        void moveWord(State *state, DisplayList **dl) {
            switch ((*dl)->p0(16, 8)) {
            case G_MW_GENSTAT:
                assert(false);
                break;
            default:
                GBI_F3DEX2::moveWord(state, dl);
                break;
            }
        }

        void rdpHalf0(State *state, DisplayList **dl) {
            uint8_t nextCode = (*dl + 1)->w0 >> 24;
            if (nextCode == S2DEX2_G_SELECT_DL) {
                assert(false);
            }
            else if (nextCode == F3DEX2_G_RDPHALF_1) {
                GBI_RDP::texrect(state, dl);
            }
        }

        void reset(State *state) {
            state->rsp->objRenderMode = 0x0;
            state->rsp->S2D.struct_buffer.fill(0);
            state->rsp->S2D.statuses.fill(0);
            state->rsp->S2D.data_02AE = -128;
        }

        void resetFromLoad(State* state) {
            state->rsp->objRenderMode = 0x0;
            state->rsp->S2D.struct_buffer.fill(0);
            state->rsp->S2D.statuses.fill(0);
            state->rsp->S2D.data_02AE = -128;
        }

        void setup(GBI *gbi) {
            gbi->constants = {
                { F3DENUM::G_MTX_MODELVIEW, 0x00 },
                { F3DENUM::G_MTX_PROJECTION, 0x04 },
                { F3DENUM::G_MTX_MUL, 0x00 },
                { F3DENUM::G_MTX_LOAD, 0x02 },
                { F3DENUM::G_MTX_NOPUSH, 0x00 },
                { F3DENUM::G_MTX_PUSH, 0x01 },
                { F3DENUM::G_TEXTURE_ENABLE, 0x00000000 },
                { F3DENUM::G_SHADING_SMOOTH, 0x00200000 },
                { F3DENUM::G_CULL_FRONT, 0x00000200 },
                { F3DENUM::G_CULL_BACK, 0x00000400 },
                { F3DENUM::G_CULL_BOTH, 0x00000600 }
            };

            gbi->map[F3DEX2_G_SPNOOP] = &GBI_EXTENDED::noOpHook;
            gbi->map[S2DEX2_G_OBJ_RENDERMODE] = &GBI_S2DEX::objRenderMode;
            gbi->map[S2DEX2_G_BG_1CYC] = &GBI_S2DEX::bg1Cyc;
            gbi->map[S2DEX2_G_BG_COPY] = &GBI_S2DEX::bgCopy;
            gbi->map[S2DEX2_G_OBJ_LOADTXTR] = &GBI_S2DEX::objLoadTxtr;
            gbi->map[S2DEX2_G_OBJ_LDTX_SPRITE] = &GBI_S2DEX::objLoadTxSprite;
            gbi->map[S2DEX2_G_OBJ_LDTX_RECT] = &GBI_S2DEX::objLoadTxRect;
            gbi->map[S2DEX2_G_OBJ_LDTX_RECT_R] = &GBI_S2DEX::objLoadTxRectR;
            gbi->map[F3DEX2_G_DL] = &GBI_F3D::runDl;
            gbi->map[F3DEX2_G_MOVEWORD] = &moveWord;
            gbi->map[F3DEX2_G_SETOTHERMODE_H] = &GBI_F3DEX2::setOtherModeH;
            gbi->map[F3DEX2_G_SETOTHERMODE_L] = &GBI_F3DEX2::setOtherModeL;
            gbi->map[F3DEX2_G_ENDDL] = &GBI_F3D::endDl;
            gbi->map[S2DEX2_G_RDPHALF_0] = &rdpHalf0;
            gbi->map[F3DEX2_G_RDPHALF_1] = &GBI_F3D::rdpHalf1;
            gbi->map[F3DEX2_G_RDPHALF_2] = &GBI_F3D::rdpHalf2;
            gbi->map[F3DEX2_G_LOAD_UCODE] = &GBI_F3DEX::loadUCode;
            gbi->map[G_RDPSETOTHERMODE] = &GBI_F3DEX2::setOtherMode;
            gbi->map[G_SETCIMG] = &GBI_F3D::setColorImage;
            gbi->map[G_SETZIMG] = &GBI_F3D::setDepthImage;
            gbi->map[G_SETTIMG] = &GBI_F3D::setTextureImage;

            gbi->resetFromTask = &reset;
            gbi->resetFromLoad = &resetFromLoad;
        }
    }
};