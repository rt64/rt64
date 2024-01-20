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
        float3 direction;
        float3 diffuseColor;
        float attenuationRadius;
        float pointRadius;
        float spotFalloffCosine;
        float spotMaxCosine;
        float3 specularColor;
        float shadowOffset;
        float attenuationExponent;
        float flickerIntensity;
        uint groupBits;
    };
#ifdef HLSL_CPU
};
#endif