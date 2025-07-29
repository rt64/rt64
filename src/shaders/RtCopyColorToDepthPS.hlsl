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
Texture2DMS<float4> gInput : register(t1);
#else
Texture2D<float4> gInput : register(t1);
#endif

float4 PSMain(in float4 pos : SV_Position, in float2 uv : TEXCOORD0, out float resultDepth : SV_DEPTH) : SV_TARGET {
#ifdef MULTISAMPLING
    // Use the closest resulting depth.
    resultDepth = 1.0f;
    
#if defined(SAMPLES_8X)
    const uint sampleCount = 8;
#elif defined(SAMPLES_4X)
    const uint sampleCount = 4;
#elif defined(SAMPLES_2X)
    const uint sampleCount = 2;
#endif
    
    for (uint i = 0; i < sampleCount; i++) {
        float4 inputColor = gInput.Load(pos.xy, i);
        uint rgba16 = Float4ToRGBA16(inputColor, 0, gConstants.usesHDR);
        resultDepth = min(Depth16ToFloat(rgba16), resultDepth);
    }
#else
    float4 inputColor = gInput.Load(uint3(pos.xy, 0));
    uint rgba16 = Float4ToRGBA16(inputColor, 0, gConstants.usesHDR);
    resultDepth = Depth16ToFloat(rgba16);
#endif
    
    return 0.0f;
}