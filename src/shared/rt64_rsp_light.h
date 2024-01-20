//
// RT64
//

#pragma once

#include "shared/rt64_hlsl.h"

#ifdef HLSL_CPU
namespace interop {
#endif
    struct RSPLight {
        float3 posDir;
        float3 col;
        float3 colc;
        uint kc;
        uint kl;
        uint kq;
    };

#ifndef HLSL_CPU
    float computeAttenuation(float distance, const RSPLight light) {
        const float A = 1.0f / 2.0f;
        const float B = 1.0f / 16.0f;
        const float C = 1.0f / 32768.0f;
        const float D = 1.0f / 524288.0f;
        const float Epsilon = 1e-6f;
        float attenuation = 1.0f / (A + B * light.kc + C * light.kl * distance + D * light.kq * pow(distance, 2.0f));
        attenuation = fmod(attenuation, 1.0f + Epsilon);
        return attenuation;
    }

    // This NdotL formula matches microcode behavior.
    float computeNDotL(float3 norm, float3 lightDir) {
        return clamp(dot(norm, lightDir) * 4.0f, 0.0f, 1.0f);
    }

    // This length formula matches microcode behavior.
    float computeLength(float3 d) {
        return sqrt(d.x * d.x + d.y * d.y + 2.0f * d.z * d.z);
    }

    float3 computePosLight(const float3 pos, const float3 norm, const RSPLight light, const float4x4 worldMatrix) {
        const float3 worldVertexPos = mul(worldMatrix, float4(pos, 1.0f)).xyz;
        float3 worldLightDir = light.posDir - worldVertexPos;
        const float worldLightDist = computeLength(worldLightDir);
        if (worldLightDist > 0) {
            worldLightDir /= worldLightDist;
        }

        const float3 localLightDir = mul(float4(worldLightDir, 0.0f), worldMatrix).xyz;
        return computeNDotL(norm, localLightDir) * computeAttenuation(worldLightDist, light) * light.col;
    }

    float3 computeDirLight(const float3 norm, const RSPLight light, const float4x4 worldMatrix) {
        float3 localLightDir = mul(float4(light.posDir, 0.0f), worldMatrix).xyz;
        float localLightLength = length(localLightDir);
        if (localLightLength > 0) {
            localLightDir /= localLightLength;
        }

        return max(dot(norm, localLightDir), 0.0f) * light.col;
    }
#endif
#ifdef HLSL_CPU
};
#endif