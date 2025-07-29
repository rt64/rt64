//
// RT64
//

#include "shared/rt64_render_target_copy.h"

#include "Depth.hlsli"
#include "Formats.hlsli"

[[vk::push_constant]] ConstantBuffer<RenderTargetCopyCB> gConstants : register(b0);

#if defined(SAMPLES_8X) || defined(SAMPLES_4X) || defined(SAMPLES_2X)
#   define MULTISAMPLING
#endif

#ifdef MULTISAMPLING
Texture2DMS<float> gInput : register(t1);
#else
Texture2D<float> gInput : register(t1);
#endif

float4 PSMain(in float4 pos : SV_Position, in float2 uv : TEXCOORD0) : SV_TARGET {
#ifdef MULTISAMPLING
    // Use the closest input depth.
    float inputDepth = 1.0f;
    
#if defined(SAMPLES_8X)
    const uint sampleCount = 8;
#elif defined(SAMPLES_4X)
    const uint sampleCount = 4;
#elif defined(SAMPLES_2X)
    const uint sampleCount = 2;
#endif
    
    for (uint i = 0; i < sampleCount; i++) {
        inputDepth = min(gInput.Load(pos.xy, i), inputDepth);
    }
#else
    float inputDepth = gInput.Load(uint3(pos.xy, 0));
#endif
    
    uint depth16 = FloatToDepth16(inputDepth, 0.0f);
    return RGBA16ToFloat4(depth16);
}