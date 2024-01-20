//
// RT64
//

// Adapted from https://www.shadertoy.com/view/3dlGDM

#include "Color.hlsli"
#include "FbRendererCommon.hlsli"
#include "FbRendererRT.hlsli"
#include "Random.hlsli"

float2 getPreviousFrameUVs(float2 pos) {
    uint2 posInt = uint2(round(pos));
    float2 flow = gFlow[posInt].xy;
    return pos + flow;
}

float distanceFromLineSegment(float2 p, float2 start, float2 end) {
    float len = length(start - end);
    float l2 = len * len;
    if (l2 == 0.0f) {
        return length(p - start);
    }

    float t = max(0.0f, min(1.0f, dot(p - start, end - start) / l2));
    float2 projection = start + t * (end - start);
    return length(p - projection);
}

float4 getMotionVector(float2 pos) {
    float lineThickness = 1.0f;
    float blockSize = 32.0f;

    // Divide the frame into blocks of "blockSize x blockSize" pixels
    // and get the screen coordinates of the pixel in the center of closest block.
    float2 currentCenterPos = floor(pos / blockSize) * blockSize + (blockSize * 0.5f);

    // Load motion vector for pixel in the center of the block.
    float2 previousCenterPos = getPreviousFrameUVs(currentCenterPos);

    // Get distance of this pixel from motion vector line on the screen.
    float lineDistance = distanceFromLineSegment(pos, currentCenterPos, previousCenterPos);

    // Draw line based on distance.
    return (lineDistance < lineThickness) ? float4(1.0f, 1.0f, 1.0f, 1.0f) : float4(0.0f, 0.0f, 0.0f, 0.0f);
}

float4 getShadingPosition(float2 pos) {
    return float4(gShadingPosition[pos].rgb, 1.0f);
}

float4 getShadingNormal(float2 pos) {
    return float4((gShadingNormal[pos].rgb + 1.0f) / 2.0f, 1.0f);
}

float4 getShadingSpecular(float2 pos) {
    return float4(gShadingSpecular[pos].rgb, 1.0f);
}

float4 getDiffuse(float2 pos) {
    return float4(LinearToSrgb(gDiffuse[pos].rgb), 1.0f);
}

float4 getInstanceId(float2 pos) {
    int instanceId = gInstanceId[pos];
    if (instanceId >= 0) {
        uint seed = initRand(instanceId, 0, 16);
        return float4(nextRand(seed), nextRand(seed), nextRand(seed), 1.0f);
    }
    else {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }
}

float4 getDirectLightRaw(float2 pos) {
    return float4(LinearToSrgb(gDirectLightAccum[pos].rgb), 1.0f);
}

float4 getDirectLightFiltered(float2 pos) {
    return float4(LinearToSrgb(gFilteredDirectLight[pos].rgb), 1.0f);
}

float4 getIndirectLightRaw(float2 pos) {
    return float4(LinearToSrgb(gIndirectLightAccum[pos].rgb), 1.0f);
}

float4 getIndirectLightFiltered(float2 pos) {
    return float4(LinearToSrgb(gFilteredIndirectLight[pos].rgb), 1.0f);
}

float4 getReflection(float2 pos) {
    return float4(gReflection[pos].rgb, 1.0f);
}

float4 getRefraction(float2 pos) {
    return float4(gRefraction[pos].rgb, 1.0f);
}

float4 getTransparent(float2 pos) {
    return float4(gTransparent[pos].rgb, 1.0f);
}

float4 getReactiveMask(float2 pos) {
    float r = gReactiveMask[pos];
    return float4(r, r, r, 1.0f);
}

float4 getLockMask(float2 pos) {
    float l = gLockMask[pos];
    return float4(l, l, l, 1.0f);
}

float4 getDepth(float2 pos) {
    float d = gDepth[pos].r;
    return float4(d, d, d, 1.0f);
}

float4 PSMain(in float4 pos : SV_Position, in float2 uv : TEXCOORD0) : SV_TARGET {
    switch (RtParams.visualizationMode) {
    case VisualizationMode::ShadingPosition:
        return getShadingPosition(uv * RtParams.resolution.xy);
    case VisualizationMode::ShadingNormal:
        return getShadingNormal(uv * RtParams.resolution.xy);
    case VisualizationMode::ShadingSpecular:
        return getShadingSpecular(uv * RtParams.resolution.xy);
    case VisualizationMode::Diffuse:
        return getDiffuse(uv * RtParams.resolution.xy);
    case VisualizationMode::InstanceId:
        return getInstanceId(uv * RtParams.resolution.xy);
    case VisualizationMode::DirectLightRaw:
        return getDirectLightRaw(uv * RtParams.resolution.xy);
    case VisualizationMode::DirectLightFiltered:
        return getDirectLightFiltered(uv * RtParams.resolution.xy);
    case VisualizationMode::IndirectLightRaw:
        return getIndirectLightRaw(uv * RtParams.resolution.xy);
    case VisualizationMode::IndirectLightFiltered:
        return getIndirectLightFiltered(uv * RtParams.resolution.xy);
    case VisualizationMode::Reflection:
        return getReflection(uv * RtParams.resolution.xy);
    case VisualizationMode::Refraction:
        return getRefraction(uv * RtParams.resolution.xy);
    case VisualizationMode::Transparent:
        return getTransparent(uv * RtParams.resolution.xy);
    case VisualizationMode::Flow:
        return getMotionVector(uv * RtParams.resolution.xy);
    case VisualizationMode::ReactiveMask:
        return getReactiveMask(uv * RtParams.resolution.xy);
    case VisualizationMode::LockMask:
        return getLockMask(uv * RtParams.resolution.xy);
    case VisualizationMode::Depth:
        return getDepth(uv * RtParams.resolution.xy);
    default:
        return float4(0.5f, 0.5f, 0.5f, 1.0f);
    }
}