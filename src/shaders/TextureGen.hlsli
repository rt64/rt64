//
// RT64
//

#pragma once

#include "shared/rt64_rsp_lookat.h"

float3 normalizeSafe(float3 v) {
    float l = length(v);
    if (l > 0) {
        return v / l;
    }
    else {
        return v;
    }
}

float2 computeTextureGen(float2 inputUV, float3 inputNormal, RSPLookAt lookAt, bool textureGenLinear, const float4x4 worldMatrix) {
    float2 texgenUV;
    texgenUV.x = dot(inputNormal, normalizeSafe(mul(float4(lookAt.x, 0.0f), worldMatrix).xyz));
    texgenUV.y = dot(inputNormal, normalizeSafe(mul(float4(lookAt.y, 0.0f), worldMatrix).xyz));
    texgenUV = clamp(texgenUV, float2(-1.0f, -1.0f), float2(1.0f, 1.0f));
    if (textureGenLinear) {
        texgenUV = acos(-texgenUV) * 325.94932f; // 1024 / PI
    }
    else {
        texgenUV += float2(1.0f, 1.0f);
        texgenUV *= 512.0f;
    }

    // Texture scaling is encoded into UV directly when texture gen is enabled.
    return (inputUV / 65536.0f) * texgenUV;
}