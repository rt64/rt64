//
// RT64
//

#include "Color.hlsli"
#include "Constants.hlsli"
#include "Math.hlsli"

#include "shared/rt64_raytracing_params.h"

#define ENABLE_EXPOSURE_ADJUSTMENT

ConstantBuffer<RaytracingParams> RtParams : register(b0);
Texture2D<float4> gInput : register(t1);
Texture2D<float4> gFlow : register(t2);
Texture2D<float> gLumaAvg : register(t3);
SamplerState gSampler : register(s4);

float4 ColorMotionBlurred(float2 uv) {
    if ((RtParams.motionBlurStrength > 0.0f) && (RtParams.motionBlurSamples > 0)) {
        float2 flow = gFlow.SampleLevel(gSampler, uv, 0).xy / RtParams.resolution.xy;
        float flowLength = length(flow);
        if (flowLength > EPSILON) {
            const float SampleStep = RtParams.motionBlurStrength / RtParams.motionBlurSamples;
            float3 sumColor = float3(0.0f, 0.0f, 0.0f);
            float sumWeight = 0.0f;
            float2 startUV = uv - (flow * RtParams.motionBlurStrength / 2.0f);
            for (uint s = 0; s < RtParams.motionBlurSamples; s++) {
                float2 sampleUV = clamp(startUV + flow * s * SampleStep, float2(0.0f, 0.0f), float2(1.0f, 1.0f));
                float sampleWeight = 1.0f;
                float4 outputColor = gInput.SampleLevel(gSampler, sampleUV, 0);
                sumColor += outputColor.rgb * sampleWeight;
                sumWeight += sampleWeight;
            }

            return float4(sumColor / sumWeight, 1.0f);
        }
    }

    float4 outputColor = gInput.SampleLevel(gSampler, uv, 0);
    return float4(outputColor.rgb, 1.0f);
}

float3 WhiteBlackPoint(float3 bl, float3 wp, float3 color) {
    return (color - bl) / (wp - bl);
}

float4 PSMain(in float4 pos : SV_Position, in float2 uv : TEXCOORD0) : SV_TARGET {
    // For the reasons stated in ComposePS, the exposure adjustment is performed in sRGB space and to 
    // make the histogram have a better distribution since it doesn't use a logarithmic scale.
    float4 color = max(ColorMotionBlurred(uv), 0.0f);

#ifdef ENABLE_EXPOSURE_ADJUSTMENT
    float avgLuma = gLumaAvg[uint2(0, 0)];
    float exposure = RtParams.tonemapExposure / avgLuma;
    color.rgb *= exposure;
    color.rgb = WhiteBlackPoint(RtParams.tonemapBlack, RtParams.tonemapWhite, color.rgb);
#endif

    return clamp(color, 0.0f, 1.0f);
}