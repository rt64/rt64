//
// RT64
//

#include "shared/rt64_render_target_copy.h"

#include "Depth.hlsli"
#include "Formats.hlsli"

[[vk::push_constant]] ConstantBuffer<RenderTargetCopyCB> gConstants : register(b0);

#ifdef MULTISAMPLING
Texture2DMS<float4> gInput : register(t1);
#else
Texture2D<float4> gInput : register(t1);
#endif

float4 PSMain(in float4 pos : SV_Position, in float2 uv : TEXCOORD0, in uint sampleIndex : SV_SampleIndex, out float resultDepth : SV_DEPTH) : SV_TARGET {
#ifdef MULTISAMPLING
    float4 inputColor = gInput.Load(pos.xy, sampleIndex);
#else
    float4 inputColor = gInput.Load(uint3(pos.xy, 0));
#endif
    uint rgba16 = Float4ToRGBA16(inputColor, 0, gConstants.usesHDR);
    resultDepth = Depth16ToFloat(rgba16);
    return 0.0f;
}