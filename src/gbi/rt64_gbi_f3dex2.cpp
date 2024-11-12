//
// RT64
//

#include "rt64_gbi_f3dex2.h"

#include <cassert>

#include "../include/rt64_extended_gbi.h"
#include "hle/rt64_interpreter.h"

#include "rt64_gbi_extended.h"
#include "rt64_gbi_f3d.h"
#include "rt64_gbi_f3dex.h"

namespace RT64 {
    namespace GBI_F3DEX2 {
        void setOtherMode(State *state, DisplayList **dl) {
            const uint32_t high = (*dl)->p0(0, 24);
            const uint32_t low = (*dl)->w1;
            state->rsp->setOtherMode(high, low);
        }

        void setOtherModeH(State *state, DisplayList **dl) {
            const uint32_t size = (*dl)->p0(0, 8) + 1;
            const uint32_t off = std::max(0, (int32_t)(32 - (*dl)->p0(8, 8) - size));
            state->rsp->setOtherModeH(size, off, (*dl)->w1);
        }

        void setOtherModeL(State *state, DisplayList **dl) {
            const uint32_t size = (*dl)->p0(0, 8) + 1;
            const uint32_t off = std::max(0, (int32_t)(32 - (*dl)->p0(8, 8) - size));
            state->rsp->setOtherModeL(size, off, (*dl)->w1);
        }

        void moveMem(State *state, DisplayList **dl) {
            uint8_t index = (*dl)->p0(0, 8);
            switch (index) {
            case F3DEX2_G_MV_VIEWPORT:
                state->rsp->setViewport((*dl)->w1);
                break;
            case F3DEX2_G_MV_MATRIX:
                state->rsp->forceMatrix((*dl)->w1);
                break;
            case F3DEX2_G_MV_LIGHT: {
                uint8_t offset = (*dl)->p0(8, 8) * 8;
                int index = (offset / 24);
                if (index >= 2) {
                    state->rsp->setLight(index - 2, (*dl)->w1);
                }
                else {
                    state->rsp->setLookAt(index, (*dl)->w1);
                }

                break;
            }
            default:
                assert(false && "Unimplemented moveMem command");
                break;
            }
        }
        
        void moveWord(State *state, DisplayList **dl) {
            uint8_t type = (*dl)->p0(16, 8);
            switch (type) {
            case F3DEX2_G_MW_FORCEMTX:
                state->rsp->setModelViewProjChanged((*dl)->w1 == 0);
                break;
            case G_MW_MATRIX:
                state->rsp->insertMatrix((*dl)->p0(0, 16), (*dl)->w1);
                break;
            case G_MW_NUMLIGHT:
                state->rsp->setLightCount((*dl)->w1 / 24);
                break;
            case G_MW_CLIP:
                state->rsp->setClipRatio((*dl)->w1);
                break;
            case G_MW_SEGMENT:
                state->rsp->setSegment((*dl)->p0(2, 4), (*dl)->w1);
                break;
            case G_MW_FOG:
                state->rsp->setFog((int16_t)((*dl)->p1(16, 16)), (int16_t)((*dl)->p1(0, 16)));
                break;
            case G_MW_LIGHTCOL:
                state->rsp->setLightColor(((*dl)->p0(0, 16) / 24), (*dl)->w1);
                break;
            case G_MW_PERSPNORM:
                state->rsp->setPerspNorm((*dl)->w1);
                break;
            default:
                assert(false && "Unimplemented moveWord command");
                break;
            }
        }

        void matrix(State *state, DisplayList **dl) {
            state->rsp->matrix((*dl)->w1, (*dl)->p0(0, 8) ^ state->rsp->pushMask);
        }

        void popMatrix(State *state, DisplayList **dl) {
            state->rsp->popMatrix((*dl)->w1 >> 6);
        }

        void geometryMode(State *state, DisplayList **dl) {
            uint32_t offMask = (*dl)->p0(0, 24);
            uint32_t onMask = (*dl)->w1;
            state->rsp->modifyGeometryMode(offMask, onMask);
        }

        void texture(State *state, DisplayList **dl) {
            uint8_t tile = (*dl)->p0(8, 3);
            uint8_t level = (*dl)->p0(11, 3);
            uint8_t on = (*dl)->p0(1, 7);
            uint16_t sc = (*dl)->p1(16, 16);
            uint16_t tc = (*dl)->p1(0, 16);
            state->rsp->setTexture(tile, level, on, sc, tc);
        }

        void dmaIO(State *state, DisplayList **dl) {
            // Do nothing. This is not possible to implement without RSP computations in the CPU.
        }

        void special1(State *state, DisplayList **dl) {
            const uint32_t param = (*dl)->p0(0, 8);
            if (state->ext.interpreter->hleGBI->flags.computeMVP) {
                if (param == 1) {
                    state->rsp->specialComputeModelViewProj();
                }
                else {
                    assert(false && "Unimplemented combine matrices mode");
                }
            }
            else {
                assert(false && "Unimplemented special1 command");
            }
        }

        void vertex(State *state, DisplayList **dl) {
            uint8_t vtxCount = (*dl)->p0(12, 8);
            state->rsp->setVertex((*dl)->w1, vtxCount, (*dl)->p0(1, 7) - vtxCount);
        }

        void tri1(State *state, DisplayList **dl) {
            state->rsp->drawIndexedTri((*dl)->p0(17, 7), (*dl)->p0(9, 7), (*dl)->p0(1, 7));
        }

        void tri2(State *state, DisplayList **dl) {
            state->rsp->drawIndexedTri((*dl)->p0(17, 7), (*dl)->p0(9, 7), (*dl)->p0(1, 7));
            state->rsp->drawIndexedTri((*dl)->p1(17, 7), (*dl)->p1(9, 7), (*dl)->p1(1, 7));
        }

        void quad(State *state, DisplayList **dl) {
            tri2(state, dl);
        }

        void line3D(State *state, DisplayList **dl) {
            // TODO
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

            gbi->map[F3DEX2_G_RDPHALF_1] = &GBI_F3D::rdpHalf1;
            gbi->map[F3DEX2_G_RDPHALF_2] = &GBI_F3D::rdpHalf2;
            gbi->map[F3DEX2_G_SETOTHERMODE_H] = &setOtherModeH;
            gbi->map[F3DEX2_G_SETOTHERMODE_L] = &setOtherModeL;
            gbi->map[F3DEX2_G_SPNOOP] = &GBI_EXTENDED::noOpHook;
            gbi->map[F3DEX2_G_DL] = &GBI_F3D::runDl;
            gbi->map[F3DEX2_G_ENDDL] = &GBI_F3D::endDl;
            gbi->map[F3DEX2_G_LOAD_UCODE] = &GBI_F3DEX::loadUCode;
            gbi->map[F3DEX2_G_MOVEMEM] = &moveMem;
            gbi->map[F3DEX2_G_MOVEWORD] = &moveWord;
            gbi->map[F3DEX2_G_MTX] = &matrix;
            gbi->map[F3DEX2_G_POPMTX] = &popMatrix;
            gbi->map[F3DEX2_G_GEOMETRYMODE] = &geometryMode;
            gbi->map[F3DEX2_G_TEXTURE] = &texture;
            gbi->map[F3DEX2_G_DMA_IO] = &dmaIO;
            gbi->map[F3DEX2_G_SPECIAL_1] = &special1;
            gbi->map[F3DEX2_G_VTX] = &vertex;
            gbi->map[F3DEX2_G_MODIFYVTX] = &GBI_F3DEX::modifyVertex;
            gbi->map[F3DEX2_G_CULLDL] = &GBI_F3DEX::cullDl;
            gbi->map[F3DEX2_G_BRANCH_Z] = &GBI_F3DEX::branchZ;
            gbi->map[F3DEX2_G_TRI1] = &tri1;
            gbi->map[F3DEX2_G_TRI2] = &tri2;
            gbi->map[F3DEX2_G_QUAD] = &quad;
            gbi->map[F3DEX2_G_LINE3D] = &line3D;
            gbi->map[G_RDPSETOTHERMODE] = &setOtherMode;
            gbi->map[G_SETCIMG] = &GBI_F3D::setColorImage;
            gbi->map[G_SETZIMG] = &GBI_F3D::setDepthImage;
            gbi->map[G_SETTIMG] = &GBI_F3D::setTextureImage;
        }
    }
};