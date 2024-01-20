//
// RT64
//

// Based on the shader from the D3D12 Raytracing Real Time Denoised Ambient Occlusion sample by Microsoft.
// https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12Raytracing/src/D3D12RaytracingRealTimeDenoisedAmbientOcclusion/RTAO/Shaders/Denoising/GaussianFilterRG3x3CS.hlsl
// Copyright (c) Microsoft. All rights reserved.

#define BLOCK_SIZE 8

struct TextureCB {
    uint2 TextureSize;
    float2 TexelSize;
};

[[vk::push_constant]] ConstantBuffer<TextureCB> gConstants : register(b0);
Texture2D<float4> gInput : register(t1);
RWTexture2D<float4> gOutput : register(u2);
SamplerState gSampler : register(s3);

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void CSMain(uint2 DTid : SV_DispatchThreadID) {
    float4 weights;

    // Non-border pixels
    if (DTid.x > 0 && DTid.y > 0 && DTid.x < gConstants.TextureSize.x - 1 && DTid.y < gConstants.TextureSize.y - 1) {
        weights = float4(0.077847 + 0.123317 + 0.123317 + 0.195346,
            0.077847 + 0.123317,
            0.077847 + 0.123317,
            0.077847);
    }
    // Top-left corner
    else if (DTid.x == 0 && DTid.y == 0) {
        weights = float4(0.195346, 0.123317, 0.123317, 0.077847) / 0.519827;
    }
    // Top-right corner
    else if (DTid.x == gConstants.TextureSize.x - 1 && DTid.y == 0) {
        weights = float4(0.123317 + 0.195346, 0, 0.201164, 0) / 0.519827;
    }
    // Bottom-left corner
    else if (DTid.x == 0 && DTid.y == gConstants.TextureSize.y - 1) {
        weights = float4(0.123317 + 0.195346, 0.077847 + 0.123317, 0, 0) / 0.519827;
    }
    // Bottom-right corner
    else if (DTid.x == gConstants.TextureSize.x - 1 && DTid.y == gConstants.TextureSize.y - 1) {
        weights = float4(0.077847 + 0.123317 + 0.123317 + 0.195346, 0, 0, 0) / 0.519827;
    }
    // Left border
    else if (DTid.x == 0) {
        weights = float4(0.123317 + 0.195346, 0.077847 + 0.123317, 0.123317, 0.077847) / 0.720991;
    }
    // Right border
    else if (DTid.x == gConstants.TextureSize.x - 1) {
        weights = float4(0.077847 + 0.123317 + 0.123317 + 0.195346, 0, 0.077847 + 0.123317, 0) / 0.720991;
    }
    // Top border
    else if (DTid.y == 0) {
        weights = float4(0.123317 + 0.195346, 0.123317, 0.077847 + 0.123317, 0.077847) / 0.720991;
    }
    // Bottom border
    else {
        weights = float4(0.077847 + 0.123317 + 0.123317 + 0.195346, 0.077847 + 0.123317, 0, 0) / 0.720991;
    }

    const float2 offsets[3] = {
        float2(0.5, 0.5) + float2(-0.123317 / (0.123317 + 0.195346), -0.123317 / (0.123317 + 0.195346)),
        float2(0.5, 0.5) + float2(1, -0.077847 / (0.077847 + 0.123317)),
        float2(0.5, 0.5) + float2(-0.077847 / (0.077847 + 0.123317), 1) };

    float4 samples[4];
    samples[0] = gInput.SampleLevel(gSampler, (DTid + offsets[0]) * gConstants.TexelSize, 0);
    samples[1] = gInput.SampleLevel(gSampler, (DTid + offsets[1]) * gConstants.TexelSize, 0);
    samples[2] = gInput.SampleLevel(gSampler, (DTid + offsets[2]) * gConstants.TexelSize, 0);
    samples[3] = gInput[DTid + 1];

    float4 samplesR = float4(samples[0].x, samples[1].x, samples[2].x, samples[3].x);
    float4 samplesG = float4(samples[0].y, samples[1].y, samples[2].y, samples[3].y);
    float4 samplesB = float4(samples[0].z, samples[1].z, samples[2].z, samples[3].z);
    float4 samplesA = float4(samples[0].w, samples[1].w, samples[2].w, samples[3].w);

    gOutput[DTid] = float4(dot(samplesR, weights), dot(samplesG, weights), dot(samplesB, weights), dot(samplesA, weights));
}