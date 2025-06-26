//
// RT64
//

#pragma once

#include "rt64_gbi.h"

#define F3DDKR_G_TRIN 0x05
#define F3DDKR_G_DMADL 0x07
#define F3DDKR_G_MW_BILLBOARD 0x02
#define F3DDKR_G_MW_MVMATRIX 0x0A

namespace RT64 {
    namespace GBI_F3DDKR {
        void setup(GBI *ucode);
    };
};