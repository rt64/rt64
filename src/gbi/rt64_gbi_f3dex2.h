//
// RT64
//

#pragma once

#include "rt64_gbi.h"

#define F3DEX2_G_MW_FORCEMTX 0x0c
#define F3DEX2_G_MWO_aLIGHT_2 0x18
#define F3DEX2_G_MWO_bLIGHT_2 0x1c
#define F3DEX2_G_MWO_aLIGHT_3 0x30
#define F3DEX2_G_MWO_bLIGHT_3 0x34
#define F3DEX2_G_MWO_aLIGHT_4 0x48
#define F3DEX2_G_MWO_bLIGHT_4 0x4c
#define F3DEX2_G_MWO_aLIGHT_5 0x60
#define F3DEX2_G_MWO_bLIGHT_5 0x64
#define F3DEX2_G_MWO_aLIGHT_6 0x78
#define F3DEX2_G_MWO_bLIGHT_6 0x7c
#define F3DEX2_G_MWO_aLIGHT_7 0x90
#define F3DEX2_G_MWO_bLIGHT_7 0x94
#define F3DEX2_G_MWO_aLIGHT_8 0xa8
#define F3DEX2_G_MWO_bLIGHT_8 0xac
#define F3DEX2_G_NOOP 0x00
#define F3DEX2_G_SETOTHERMODE_H 0xe3
#define F3DEX2_G_SETOTHERMODE_L 0xe2
#define F3DEX2_G_RDPHALF_1 0xe1
#define F3DEX2_G_RDPHALF_2 0xf1
#define F3DEX2_G_SPNOOP 0xe0
#define F3DEX2_G_ENDDL 0xdf
#define F3DEX2_G_DL 0xde
#define F3DEX2_G_LOAD_UCODE 0xdd
#define F3DEX2_G_MOVEMEM 0xdc
#define F3DEX2_G_MOVEWORD 0xdb
#define F3DEX2_G_MTX 0xda
#define F3DEX2_G_GEOMETRYMODE 0xd9
#define F3DEX2_G_POPMTX 0xd8
#define F3DEX2_G_TEXTURE 0xd7
#define F3DEX2_G_VTX 0x01
#define F3DEX2_G_MODIFYVTX 0x02
#define F3DEX2_G_CULLDL 0x03
#define F3DEX2_G_BRANCH_Z 0x04
#define F3DEX2_G_TRI1 0x05
#define F3DEX2_G_TRI2 0x06
#define F3DEX2_G_QUAD 0x07
#define F3DEX2_G_LINE3D 0x08
#define F3DEX2_G_DMA_IO 0xD6
#define F3DEX2_G_SPECIAL_1 0xD5
#define F3DEX2_G_MV_MMTX 2
#define F3DEX2_G_MV_PMTX 6
#define F3DEX2_G_MV_VIEWPORT 8
#define F3DEX2_G_MV_LIGHT 10
#define F3DEX2_G_MV_POINT 12
#define F3DEX2_G_MV_MATRIX 14
#define F3DEX2_G_MVO_LOOKATX (0 * 24)
#define F3DEX2_G_MVO_LOOKATY (1 * 24)
#define F3DEX2_G_MVO_L0 (2 * 24)
#define F3DEX2_G_MVO_L1 (3 * 24)
#define F3DEX2_G_MVO_L2 (4 * 24)
#define F3DEX2_G_MVO_L3 (5 * 24)
#define F3DEX2_G_MVO_L4 (6 * 24)
#define F3DEX2_G_MVO_L5 (7 * 24)
#define F3DEX2_G_MVO_L6 (8 * 24)
#define F3DEX2_G_MVO_L7 (9 * 24)

namespace RT64 {
    namespace GBI_F3DEX2 {
        void setOtherMode(State *state, DisplayList **dl);
        void setOtherModeH(State *state, DisplayList **dl);
        void setOtherModeL(State *state, DisplayList **dl);
        void moveMem(State *state, DisplayList **dl);
        void moveWord(State *state, DisplayList **dl);
        void matrix(State *state, DisplayList **dl);
        void popMatrix(State *state, DisplayList **dl);
        void geometryMode(State *state, DisplayList **dl);
        void texture(State *state, DisplayList **dl);
        void dmaIO(State *state, DisplayList **dl);
        void special1(State *state, DisplayList **dl);
        void vertex(State *state, DisplayList **dl);
        void tri1(State *state, DisplayList **dl);
        void tri2(State *state, DisplayList **dl);
        void quad(State *state, DisplayList **dl);
        void line3D(State *state, DisplayList **dl);

        void setup(GBI *gbi);
    };
};