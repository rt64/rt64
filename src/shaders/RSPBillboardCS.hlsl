//
// RT64
//

#include "shared/rt64_rsp_fog.h"

#define GROUP_SIZE 64

struct RSPBillboardCB {
    uint billboardCount;
};

[[vk::push_constant]] ConstantBuffer<RSPBillboardCB> gConstants : register(b0);
Buffer<uint> srcBillboardIndices : register(t1);
Buffer<uint> srcFogIndices : register(t2);
StructuredBuffer<RSPFog> rspFogVector : register(t3);
RWStructuredBuffer<float4> dstPos : register(u4);
RWStructuredBuffer<float4> dstCol : register(u5);

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint billboardIndex : SV_DispatchThreadID) {
    if (billboardIndex >= gConstants.billboardCount) {
        return;
    }
    
    const uint billboardOffset = billboardIndex * 2;
    const uint vertexIndex = srcBillboardIndices[billboardOffset];
    const uint anchorIndex = srcBillboardIndices[billboardOffset + 1];
    const float vertexW = dstPos[vertexIndex].w + dstPos[anchorIndex].w;
    dstPos[vertexIndex].xyz = (dstPos[vertexIndex].xyz * dstPos[vertexIndex].w + dstPos[anchorIndex].xyz * dstPos[anchorIndex].w) / vertexW;
    dstPos[vertexIndex].w = vertexW;
    
    // Fog (again).
    const uint fogIndex = srcFogIndices[vertexIndex];
    if (fogIndex > 0) {
        const RSPFog rspFog = rspFogVector[fogIndex - 1];
        const float fogAlpha = max(dstPos[vertexIndex].z, 0) * rspFog.mul + rspFog.offset;
        dstCol[vertexIndex].a = clamp(fogAlpha / 255.0f, 0.0f, 1.0f);
    }
}