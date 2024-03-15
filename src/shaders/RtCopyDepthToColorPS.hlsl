//
// RT64
//

#include "Depth.hlsli"
#include "Formats.hlsli"

#ifdef MULTISAMPLING
Texture2DMS<float> gInput : register(t0);
#else
Texture2D<float> gInput : register(t0);
#endif

float4 PSMain(in float4 pos : SV_Position, in float2 uv : TEXCOORD0, in uint sampleIndex : SV_SampleIndex) : SV_TARGET {
#ifdef MULTISAMPLING
    float inputDepth = gInput.Load(pos.xy, sampleIndex);
#else
    float inputDepth = gInput.Load(uint3(pos.xy, 0));
#endif
    uint depth16 = FloatToDepth16(inputDepth, 0.0f);
    return RGBA16ToFloat4(depth16);
}