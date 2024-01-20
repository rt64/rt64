//
// RT64
//

#pragma once

#include "rt64_hlsl.h"

#define RT64_ATTRIBUTE_NONE                         0x00000
#define RT64_ATTRIBUTE_IGNORE_NORMAL_FACTOR         0x00001
#define RT64_ATTRIBUTE_UV_DETAIL_SCALE              0x00002
#define RT64_ATTRIBUTE_REFLECTION_FACTOR            0x00004
#define RT64_ATTRIBUTE_REFLECTION_FRESNEL_FACTOR    0x00008
#define RT64_ATTRIBUTE_ROUGHNESS_FACTOR             0x00010
#define RT64_ATTRIBUTE_REFRACTION_FACTOR            0x00020
#define RT64_ATTRIBUTE_SHADOW_CATCHER_FACTOR        0x00040
#define RT64_ATTRIBUTE_SPECULAR_COLOR               0x00080
#define RT64_ATTRIBUTE_SPECULAR_EXPONENT            0x00100
#define RT64_ATTRIBUTE_SOLID_ALPHA_MULTIPLIER       0x00200
#define RT64_ATTRIBUTE_SHADOW_ALPHA_MULTIPLIER      0x00400
#define RT64_ATTRIBUTE_DEPTH_ORDER_BIAS             0x00800
#define RT64_ATTRIBUTE_SHADOW_RAY_BIAS              0x02000
#define RT64_ATTRIBUTE_SELF_LIGHT                   0x04000
#define RT64_ATTRIBUTE_LIGHT_GROUP_MASK_BITS        0x08000
#define RT64_ATTRIBUTE_DIFFUSE_COLOR_MIX            0x10000
#define RT64_ATTRIBUTE_RSP_LIGHT_DIFFUSE_MIX        0x20000
#define RT64_ATTRIBUTE_DEPTH_DECAL_BIAS             0x40000

#ifdef HLSL_CPU
namespace interop {
#endif
    struct ExtraParams {
        float rspLightDiffuseMix;
        float lockMask;
        float ignoreNormalFactor;
        float uvDetailScale;
        float reflectionFactor;
        float reflectionFresnelFactor;
        float roughnessFactor;
        float refractionFactor;
        float shadowCatcherFactor;
        float3 specularColor;
        float specularExponent;
        float solidAlphaMultiplier;
        float shadowAlphaMultiplier;
        float depthOrderBias;
        float depthDecalBias;
        float shadowRayBias;
        float3 selfLight;
        uint lightGroupMaskBits;
        float4 diffuseColorMix;
        uint enabledAttributes;

#ifdef HLSL_CPU
        void applyExtraAttributes(const ExtraParams &src) {
            if (src.enabledAttributes & RT64_ATTRIBUTE_IGNORE_NORMAL_FACTOR) {
                ignoreNormalFactor = src.ignoreNormalFactor;
            }

            if (src.enabledAttributes & RT64_ATTRIBUTE_UV_DETAIL_SCALE) {
                uvDetailScale = src.uvDetailScale;
            }

            if (src.enabledAttributes & RT64_ATTRIBUTE_REFLECTION_FACTOR) {
                reflectionFactor = src.reflectionFactor;
            }

            if (src.enabledAttributes & RT64_ATTRIBUTE_REFLECTION_FRESNEL_FACTOR) {
                reflectionFresnelFactor = src.reflectionFresnelFactor;
            }

            if (src.enabledAttributes & RT64_ATTRIBUTE_ROUGHNESS_FACTOR) {
                roughnessFactor = src.roughnessFactor;
            }

            if (src.enabledAttributes & RT64_ATTRIBUTE_REFRACTION_FACTOR) {
                refractionFactor = src.refractionFactor;
            }

            if (src.enabledAttributes & RT64_ATTRIBUTE_SHADOW_CATCHER_FACTOR) {
                shadowCatcherFactor = src.shadowCatcherFactor;
            }

            if (src.enabledAttributes & RT64_ATTRIBUTE_SPECULAR_COLOR) {
                specularColor = src.specularColor;
            }

            if (src.enabledAttributes & RT64_ATTRIBUTE_SPECULAR_EXPONENT) {
                specularExponent = src.specularExponent;
            }

            if (src.enabledAttributes & RT64_ATTRIBUTE_SOLID_ALPHA_MULTIPLIER) {
                solidAlphaMultiplier = src.solidAlphaMultiplier;
            }

            if (src.enabledAttributes & RT64_ATTRIBUTE_SHADOW_ALPHA_MULTIPLIER) {
                shadowAlphaMultiplier = src.shadowAlphaMultiplier;
            }

            if (src.enabledAttributes & RT64_ATTRIBUTE_DEPTH_ORDER_BIAS) {
                depthOrderBias = src.depthOrderBias;
            }

            if (src.enabledAttributes & RT64_ATTRIBUTE_DEPTH_DECAL_BIAS) {
                depthDecalBias = src.depthDecalBias;
            }

            if (src.enabledAttributes & RT64_ATTRIBUTE_SHADOW_RAY_BIAS) {
                shadowRayBias = src.shadowRayBias;
            }

            if (src.enabledAttributes & RT64_ATTRIBUTE_SELF_LIGHT) {
                selfLight = src.selfLight;
            }

            if (src.enabledAttributes & RT64_ATTRIBUTE_LIGHT_GROUP_MASK_BITS) {
                lightGroupMaskBits = src.lightGroupMaskBits;
            }

            if (src.enabledAttributes & RT64_ATTRIBUTE_DIFFUSE_COLOR_MIX) {
                diffuseColorMix = src.diffuseColorMix;
            }

            if (src.enabledAttributes & RT64_ATTRIBUTE_RSP_LIGHT_DIFFUSE_MIX) {
                rspLightDiffuseMix = src.rspLightDiffuseMix;
            }
        }
#endif
    };
#ifdef HLSL_CPU
};
#endif