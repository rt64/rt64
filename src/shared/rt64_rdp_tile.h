//
// RT64
//

#pragma once

#include "shared/rt64_hlsl.h"

#ifdef HLSL_CPU
namespace interop {
#endif
    struct RDPTile {
        int fmt;
        int siz;
        int stride;
        int address;
        int palette;
        int masks;
        int maskt;
        float shifts;
        float shiftt;
        float uls;
        float ult;
        float lrs;
        float lrt;
        int cms;
        int cmt;
        uint nativeSampler;
    };
#ifdef HLSL_CPU
};
#endif