//
// RT64
//

#pragma once

#include "rt64_gbi.h"

#define F3DWAVE_G_UNKNOWN       0xB4
#define F3DWAVE_G_RDPHALF_1     0xB3
#define F3DWAVE_G_RDPHALF_2     0xB2
#define F3DWAVE_G_TRI2          0xB1

namespace RT64 {
    namespace GBI_F3DWAVE {
        void setup(GBI *ucode);
    };
};