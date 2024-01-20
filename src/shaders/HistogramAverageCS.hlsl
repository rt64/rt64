//
// RT64
//
// Luminance Histogram (average pass) by Alex Tardif
// From http://www.alextardif.com/HistogramLuminance.html
//

#include "Constants.hlsli"
#include "Math.hlsli"

#define NUM_HISTOGRAM_BINS 64
#define HISTOGRAM_AVERAGE_THREADS_PER_DIMENSION 8

struct HistogramAverageCB {
    uint pixelCount;
    float minLuminance;
    float luminanceRange;
    float timeDelta;
    float tau;
};

[[vk::push_constant]] ConstantBuffer<HistogramAverageCB> gConstants : register(b0);
ByteAddressBuffer LuminanceHistogram : register(t1);
RWTexture2D<float> LuminanceOutput : register(u2);

groupshared float HistogramShared[NUM_HISTOGRAM_BINS];

[numthreads(HISTOGRAM_AVERAGE_THREADS_PER_DIMENSION, HISTOGRAM_AVERAGE_THREADS_PER_DIMENSION, 1)]
void CSMain(uint groupIndex : SV_GroupIndex) {
    float countForThisBin = (float)LuminanceHistogram.Load(groupIndex * 4);
    HistogramShared[groupIndex] = countForThisBin * (float)groupIndex;
    GroupMemoryBarrierWithGroupSync();

    [unroll]
    for (uint histogramSampleIndex = (NUM_HISTOGRAM_BINS >> 1); histogramSampleIndex > 0; histogramSampleIndex >>= 1) {
        if (groupIndex < histogramSampleIndex) {
            HistogramShared[groupIndex] += HistogramShared[groupIndex + histogramSampleIndex];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    if (groupIndex == 0) {
        float weightedAverage = (HistogramShared[0].x / max(float(gConstants.pixelCount) - countForThisBin, 1.0)) - 1.0;
        float weightedAverageLuminance = ((weightedAverage / 62.0) * gConstants.luminanceRange) + gConstants.minLuminance;
        float luminanceLastFrame = LuminanceOutput[uint2(0, 0)];
        if (isinf(luminanceLastFrame) || isnan(luminanceLastFrame)) {
            luminanceLastFrame = 1.0;
        }

        float adaptedLuminance = luminanceLastFrame + (weightedAverageLuminance - luminanceLastFrame) * (1 - exp(-gConstants.timeDelta * gConstants.tau));
        LuminanceOutput[uint2(0, 0)] = max(adaptedLuminance, EPSILON);
    }
}