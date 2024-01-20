//
// RT64
//

#pragma once

#include "rt64_gbi.h"

#define F3D_G_MW_POINTS 0x0c
#define F3D_G_MWO_aLIGHT_2 0x20
#define F3D_G_MWO_bLIGHT_2 0x24
#define F3D_G_MWO_aLIGHT_3 0x40
#define F3D_G_MWO_bLIGHT_3 0x44
#define F3D_G_MWO_aLIGHT_4 0x60
#define F3D_G_MWO_bLIGHT_4 0x64
#define F3D_G_MWO_aLIGHT_5 0x80
#define F3D_G_MWO_bLIGHT_5 0x84
#define F3D_G_MWO_aLIGHT_6 0xa0
#define F3D_G_MWO_bLIGHT_6 0xa4
#define F3D_G_MWO_aLIGHT_7 0xc0
#define F3D_G_MWO_bLIGHT_7 0xc4
#define F3D_G_MWO_aLIGHT_8 0xe0
#define F3D_G_MWO_bLIGHT_8 0xe4
#define F3D_G_NOOP 0xc0
#define F3D_G_SETOTHERMODE_H 0xBA
#define F3D_G_SETOTHERMODE_L 0xB9
#define F3D_G_RDPHALF_1 0xB4
#define F3D_G_RDPHALF_2 0xB3
#define F3D_G_SPNOOP 0x00
#define F3D_G_ENDDL 0xB8
#define F3D_G_DL 0x06
#define F3D_G_MOVEMEM 0x03
#define F3D_G_MOVEWORD 0xBC
#define F3D_G_MTX 0x01
#define F3D_G_POPMTX 0xBD
#define F3D_G_TEXTURE 0xBB
#define F3D_G_VTX 0x04
#define F3D_G_CULLDL 0xBE
#define F3D_G_TRI1 0xBF
#define F3D_G_QUAD 0xB5
#define F3D_G_SPRITE2D_BASE 0x09
#define F3D_G_SETGEOMETRYMODE 0xB7
#define F3D_G_CLEARGEOMETRYMODE 0xB6
#define F3D_G_MV_VIEWPORT 0x80
#define F3D_G_MV_LOOKATY 0x82
#define F3D_G_MV_LOOKATX 0x84
#define F3D_G_MV_L0 0x86
#define F3D_G_MV_L1 0x88
#define F3D_G_MV_L2 0x8a
#define F3D_G_MV_L3 0x8c
#define F3D_G_MV_L4 0x8e
#define F3D_G_MV_L5 0x90
#define F3D_G_MV_L6 0x92
#define F3D_G_MV_L7 0x94
#define F3D_G_MV_TXTATT 0x96
#define F3D_G_MV_MATRIX_1 0x9e
#define F3D_G_MV_MATRIX_2 0x98
#define F3D_G_MV_MATRIX_3 0x9a
#define F3D_G_MV_MATRIX_4 0x9c

namespace RT64 {
    namespace GBI_F3D {
        void matrix(State *state, DisplayList **dl);
        void moveMem(State *state, DisplayList **dl);
        void vertex(State *state, DisplayList **dl);
        void runDl(State *state, DisplayList **dl);
        void endDl(State *state, DisplayList **dl);
        void sprite2DBase(State *state, DisplayList **dl);
        void tri1(State *state, DisplayList **dl);
        void quad(State *state, DisplayList **dl);
        void cullDl(State *state, DisplayList **dl);
        void popMatrix(State *state, DisplayList **dl);
        void moveWord(State *state, DisplayList **dl);
        void texture(State *state, DisplayList **dl);
        void setOtherModeH(State *state, DisplayList **dl);
        void setOtherModeL(State *state, DisplayList **dl);
        void setGeometryMode(State *state, DisplayList **dl);
        void clearGeometryMode(State *state, DisplayList **dl);
        void rdpHalf1(State *state, DisplayList **dl);
        void rdpHalf2(State *state, DisplayList **dl);
        void setColorImage(State *state, DisplayList **dl);
        void setDepthImage(State *state, DisplayList **dl);
        void setTextureImage(State *state, DisplayList **dl);
        void reset(State *state);

        void setup(GBI *gbi);
    };
};