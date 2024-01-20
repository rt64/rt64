//
// RT64
//

#pragma once

#include "rt64_gbi.h"

#define S2DEX_G_BG_1CYC 0x01
#define S2DEX_G_BG_COPY 0x02
#define S2DEX_G_RDPHALF_0 0xE4
#define S2DEX_G_SELECT_DL 0xB0
#define S2DEX_G_OBJ_LOADTXTR 0xC1
#define S2DEX_G_OBJ_LDTX_SPRITE 0xC2
#define S2DEX_G_OBJ_LDTX_RECT 0xC3
#define S2DEX_G_OBJ_LDTX_RECT_R 0xC4
#define S2DEX_G_OBJ_RENDERMODE 0xB1
#define S2DEX_G_BGLT_LOADBLOCK 0x0033
#define S2DEX_G_BGLT_LOADTILE 0xFFF4
#define S2DEX_G_BG_FLAG_FLIPS 0x01
#define S2DEX_G_BG_FLAG_FLIPT 0x10
#define S2DEX_G_OBJRM_NOTXCLAMP 0x01
#define S2DEX_G_OBJRM_XLU 0x02
#define S2DEX_G_OBJRM_ANTIALIAS 0x04
#define S2DEX_G_OBJRM_BILERP 0x08
#define S2DEX_G_OBJRM_SHRINKSIZE_1 0x10
#define S2DEX_G_OBJRM_SHRINKSIZE_2 0x20
#define S2DEX_G_OBJRM_WIDEN 0x40

namespace RT64 {
    namespace GBI_S2DEX {
        // Structures are already endian-swapped.
#pragma pack(push,1)
        struct uObjBg_t {
            uint16_t imageW;
            uint16_t imageX;
            uint16_t frameW;
            int16_t frameX;
            uint16_t imageH;
            uint16_t imageY;
            uint16_t frameH;
            int16_t frameY;
            uint32_t imageAddress;
            uint8_t imageSiz;
            uint8_t imageFmt;
            uint16_t imageLoad;
            uint16_t imageFlip;
            uint16_t imagePal;
            uint16_t tmemH;
            uint16_t tmemW;
            uint16_t tmemLoadTH;
            uint16_t tmemLoadSH;
            uint16_t tmemSize;
            uint16_t tmemSizeW;
        };

        struct uObjScaleBg_t {
            uint16_t imageW;
            uint16_t imageX;
            uint16_t frameW;
            int16_t frameX;
            uint16_t imageH;
            uint16_t imageY;
            uint16_t frameH;
            int16_t frameY;
            uint32_t imageAddress;
            uint8_t imageSiz;
            uint8_t imageFmt;
            uint16_t imageLoad;
            uint16_t imageFlip;
            uint16_t imagePal;
            uint16_t scaleH;
            uint16_t scaleW;
            int32_t imageYorig;
            uint8_t padding[4];
        };

        struct uObjBg {
            union {
                uObjBg_t bg;
                uObjScaleBg_t scaleBg;
            };
        };

        struct uObjTxtr {
            uint32_t type;
            uint32_t image; // Segmented address
            uint16_t val1; // These two members swapped for endianness
            uint16_t tmem;
            uint16_t sid; // These two members swapped for endianness
            uint16_t val2;
            uint32_t flag;
            uint32_t mask;
        };

        struct uObjTxSprite {
            uObjTxtr txtr;
            // uObjSprite sprite;
        };
#pragma pack(pop)

        void objRenderMode(State *state, DisplayList **dl);
        void moveWord(State *state, DisplayList **dl);
        void bg1Cyc(State *state, DisplayList **dl);
        void bgCopy(State *state, DisplayList **dl);
        void objLoadTxtr(State *state, DisplayList **dl);
        void objLoadTxSprite(State* state, DisplayList** dl);
        void objLoadTxRect(State* state, DisplayList** dl);
        void objLoadTxRectR(State* state, DisplayList** dl);
        void rdpHalf0(State *state, DisplayList **dl);

        void setup(GBI *gbi);
    };
};