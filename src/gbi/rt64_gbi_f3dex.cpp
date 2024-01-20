//
// RT64
//

#include "rt64_gbi_f3dex.h"

#include "hle/rt64_interpreter.h"

#include "rt64_gbi_extended.h"
#include "rt64_gbi_f3d.h"
#include "rt64_gbi_rdp.h"

namespace RT64 {
    namespace GBI_F3DEX {
        void vertex(State *state, DisplayList **dl) {
            state->rsp->setVertex((*dl)->w1, (*dl)->p0(10, 6), (*dl)->p0(17, 7));
        }

        void modifyVertex(State *state, DisplayList **dl) {
            state->rsp->modifyVertex((*dl)->p0(1, 15), (*dl)->p0(16, 8), (*dl)->w1);
        }

        void tri1(State *state, DisplayList **dl) {
            state->rsp->drawIndexedTri((*dl)->p1(17, 7), (*dl)->p1(9, 7), (*dl)->p1(1, 7));
        }

        void tri2(State *state, DisplayList **dl) {
            state->rsp->drawIndexedTri((*dl)->p0(17, 7), (*dl)->p0(9, 7), (*dl)->p0(1, 7));
            state->rsp->drawIndexedTri((*dl)->p1(17, 7), (*dl)->p1(9, 7), (*dl)->p1(1, 7));
        }
        
        void quad(State *state, DisplayList **dl) {
            uint8_t a = (*dl)->p1(25, 7);
            uint8_t b = (*dl)->p1(17, 7);
            uint8_t c = (*dl)->p1(9, 7);
            uint8_t d = (*dl)->p1(1, 7);
            state->rsp->drawIndexedTri(a, b, c);
            state->rsp->drawIndexedTri(a, c, d);
        }

        void cullDl(State *state, DisplayList **dl) {
            // TODO
        }

        void branchZ(State *state, DisplayList **dl) {
            state->rsp->branchZ(state->microcode.half1, (*dl)->p0(1, 11), (*dl)->w1, dl);
        }
        
        void loadUCode(State *state, DisplayList **dl) {
            state->ext.interpreter->loadUCodeGBI((*dl)->w1, state->microcode.half1, false);
        }

        void setup(GBI *gbi) {
            gbi->constants = {
                { F3DENUM::G_MTX_MODELVIEW, 0x00 },
                { F3DENUM::G_MTX_PROJECTION, 0x01 },
                { F3DENUM::G_MTX_MUL, 0x00 },
                { F3DENUM::G_MTX_LOAD, 0x02 },
                { F3DENUM::G_MTX_NOPUSH, 0x00 },
                { F3DENUM::G_MTX_PUSH, 0x04 },
                { F3DENUM::G_TEXTURE_ENABLE, 0x00000002 },
                { F3DENUM::G_SHADING_SMOOTH, 0x00000200 },
                { F3DENUM::G_CULL_FRONT, 0x00001000 },
                { F3DENUM::G_CULL_BACK, 0x00002000 },
                { F3DENUM::G_CULL_BOTH, 0x00003000 }
            };

            gbi->map[F3D_G_SPNOOP] = &GBI_EXTENDED::noOpHook;
            gbi->map[F3D_G_MTX] = &GBI_F3D::matrix;
            gbi->map[F3D_G_MOVEMEM] = &GBI_F3D::moveMem;
            gbi->map[F3D_G_VTX] = &vertex;
            gbi->map[F3DEX_G_MODIFYVTX] = &modifyVertex;
            gbi->map[F3D_G_DL] = &GBI_F3D::runDl;
            gbi->map[F3D_G_ENDDL] = &GBI_F3D::endDl;
            gbi->map[F3D_G_SPRITE2D_BASE] = &GBI_F3D::sprite2DBase;
            gbi->map[F3D_G_TRI1] = &tri1;
            gbi->map[F3DEX_G_TRI2] = &tri2;
            gbi->map[F3D_G_QUAD] = &quad;
            gbi->map[F3D_G_CULLDL] = &cullDl;
            gbi->map[F3D_G_POPMTX] = &GBI_F3D::popMatrix;
            gbi->map[F3D_G_MOVEWORD] = &GBI_F3D::moveWord;
            gbi->map[F3D_G_TEXTURE] = &GBI_F3D::texture;
            gbi->map[F3D_G_SETOTHERMODE_H] = &GBI_F3D::setOtherModeH;
            gbi->map[F3D_G_SETOTHERMODE_L] = &GBI_F3D::setOtherModeL;
            gbi->map[F3D_G_SETGEOMETRYMODE] = &GBI_F3D::setGeometryMode;
            gbi->map[F3D_G_CLEARGEOMETRYMODE] = &GBI_F3D::clearGeometryMode;
            gbi->map[F3D_G_RDPHALF_1] = &GBI_F3D::rdpHalf1;
            gbi->map[F3D_G_RDPHALF_2] = &GBI_F3D::rdpHalf2;
            gbi->map[F3DEX_G_BRANCH_Z] = &branchZ;
            gbi->map[F3DEX_G_LOAD_UCODE] = &loadUCode;
            gbi->map[G_SETCIMG] = &GBI_F3D::setColorImage;
            gbi->map[G_SETZIMG] = &GBI_F3D::setDepthImage;
            gbi->map[G_SETTIMG] = &GBI_F3D::setTextureImage;
            gbi->map[G_RDPNOOP] = &GBI_RDP::noOp;

            gbi->resetFromTask = &GBI_F3D::reset;
            gbi->resetFromLoad = &GBI_F3D::reset;
        }
    }
};