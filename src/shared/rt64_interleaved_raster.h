#pragma once

#include "shared/rt64_hlsl.h"

#define MAX_INTERLEAVED_RASTERS     8

#ifdef HLSL_CPU
namespace interop {
#endif
    struct InterleavedRaster {
        uint rasterSceneIndex;
        uint firstInstanceIndex;
        uint colorTextureIndex;
        uint depthTextureIndex;
    };
#ifdef HLSL_CPU
};
#endif