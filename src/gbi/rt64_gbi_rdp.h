//
// RT64
//

#pragma once

#include "rt64_gbi.h"

namespace RT64 {
    namespace GBI_RDP {
        void noOp(State *state, DisplayList **dl);
        void setColorImage(State *state, DisplayList **dl);
        void setDepthImage(State * state, DisplayList * *dl);
        void setTextureImage(State * state, DisplayList * *dl);
        void setCombine(State * state, DisplayList * *dl);
        void setTile(State * state, DisplayList * *dl);
        void setTileSize(State * state, DisplayList * *dl);
        void loadTile(State * state, DisplayList * *dl);
        void loadBlock(State * state, DisplayList * *dl);
        void loadTLUT(State * state, DisplayList * *dl);
        void setEnvColor(State * state, DisplayList * *dl);
        void setPrimColor(State * state, DisplayList * *dl);
        void setBlendColor(State * state, DisplayList * *dl);
        void setFogColor(State * state, DisplayList * *dl);
        void setFillColor(State * state, DisplayList * *dl);
        void setOtherMode(State * state, DisplayList * *dl);
        void setPrimDepth(State * state, DisplayList * *dl);
        void setScissor(State * state, DisplayList * *dl);
        void setConvert(State *state, DisplayList **dl);
        void setKeyR(State *state, DisplayList **dl);
        void setKeyGB(State *state, DisplayList **dl);
        void texrect(State * state, DisplayList * *dl);
        void texrectFlip(State * state, DisplayList * *dl);
        void fillRect(State * state, DisplayList * *dl);
        void loadSync(State *state, DisplayList **dl);
        void pipeSync(State *state, DisplayList **dl);
        void tileSync(State *state, DisplayList **dl);
        void fullSync(State *state, DisplayList **dl);
        void setup(GBI *gbi, bool isHLE);
    };
};