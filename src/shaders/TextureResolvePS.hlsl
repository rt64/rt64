//
// RT64
//

#include "shared/rt64_texture_copy.h"

[[vk::push_constant]] ConstantBuffer<TextureCopyCB> gConstants : register(b0);

Texture2DMS<float4> gInput : register(t1);

float4 PSMain(in float4 pos : SV_Position, in float2 uv : TEXCOORD0) : SV_TARGET {
    uint2 pixelPos = gConstants.uvScroll + uv.xy * gConstants.uvScale;
#if defined(SAMPLES_8X)
    return (
        gInput.Load(pixelPos, 0) + 
        gInput.Load(pixelPos, 1) +
        gInput.Load(pixelPos, 2) + 
        gInput.Load(pixelPos, 3) +
        gInput.Load(pixelPos, 4) + 
        gInput.Load(pixelPos, 5) + 
        gInput.Load(pixelPos, 6) +
        gInput.Load(pixelPos, 7)
    ) / 8.0f;
#elif defined(SAMPLES_4X)
    return (
        gInput.Load(pixelPos, 0) + 
        gInput.Load(pixelPos, 1) +
        gInput.Load(pixelPos, 2) + 
        gInput.Load(pixelPos, 3)
    ) / 4.0f;
#elif defined(SAMPLES_2X)
    return (
        gInput.Load(pixelPos, 0) + 
        gInput.Load(pixelPos, 1)
    ) / 2.0f;
#else
    return gInput.Load(pixelPos, 0);
#endif
}