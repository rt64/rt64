//
// RT64
//

#pragma once

#include "shared/rt64_hlsl.h"

#ifdef HLSL_CPU
namespace interop {
#endif
    struct PointLight {
        float3 position;
        float attenuationRadius;
        float3 direction;
        float pointRadius;
        float3 diffuseColor;
        float spotFalloffCosine;
        float spotMaxCosine;
        float shadowOffset;
        float attenuationExponent;
        float flickerIntensity;
        float3 specularColor;
        uint groupBits;
    };
#ifdef HLSL_CPU
};
#endif