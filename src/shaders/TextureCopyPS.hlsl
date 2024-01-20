//
// RT64
//

#include "shared/rt64_texture_copy.h"

[[vk::push_constant]] ConstantBuffer<TextureCopyCB> gConstants : register(b0);

Texture2D<float4> gInput : register(t1);

float4 PSMain(in float4 pos : SV_Position, in float2 uv : TEXCOORD0) : SV_TARGET {
    uint2 pixelPos = gConstants.uvScroll + uv.xy * gConstants.uvScale;
    return gInput.Load(uint3(pixelPos, 0));
}