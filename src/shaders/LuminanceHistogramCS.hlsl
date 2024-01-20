//
//  RT64
//
//  Luminance Histogram by Alex Tardif
//  From http://www.alextardif.com/HistogramLuminance.html
//

#include "Color.hlsli"

#define NUM_HISTOGRAM_BINS 64
#define HISTOGRAM_THREADS_PER_DIMENSION 8
#define EPSILON 1e-6

struct LuminanceHistogramCB {
    uint inputWidth;
    uint inputHeight;
    float minLuminance;
    float oneOverLuminanceRange;
};

[[vk::push_constant]] ConstantBuffer<LuminanceHistogramCB> gConstants : register(b0);
Texture2D<float3> HDRTexture : register(t1);
RWByteAddressBuffer LuminanceHistogram : register(u2);

groupshared uint HistogramShared[NUM_HISTOGRAM_BINS];

float GetLuminance(float3 color) {
    return dot(color, float3(0.2127f, 0.7152f, 0.0722f));
}

uint HDRToHistogramBin(float3 hdrColor)
{
    float luminance = GetLuminance(hdrColor);
    if (luminance < EPSILON) {
        return 0;
    }

    float satLuminance = saturate((luminance - gConstants.minLuminance) * gConstants.oneOverLuminanceRange);
    return (uint) (satLuminance * 62.0 + 1.0);
}

[numthreads(HISTOGRAM_THREADS_PER_DIMENSION, HISTOGRAM_THREADS_PER_DIMENSION, 1)]
void CSMain(uint groupIndex : SV_GroupIndex, uint3 threadId : SV_DispatchThreadID) {
    HistogramShared[groupIndex] = 0;

    GroupMemoryBarrierWithGroupSync();
    if (threadId.x < gConstants.inputWidth && threadId.y < gConstants.inputHeight) {
        float3 hdrColor = HDRTexture.Load(int3(threadId.xy, 0)).rgb;
        uint binIndex = HDRToHistogramBin(hdrColor);
        InterlockedAdd(HistogramShared[binIndex], 1);
    }
    GroupMemoryBarrierWithGroupSync();

    LuminanceHistogram.InterlockedAdd(groupIndex * 4, HistogramShared[groupIndex]);
}