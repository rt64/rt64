//
// RT64
//

#include "shared/rt64_rsp_fog.h"
#include "shared/rt64_rsp_light.h"
#include "shared/rt64_rsp_lookat.h"
#include "shared/rt64_rsp_viewport.h"

#include "TextureGen.hlsli"

#define GROUP_SIZE 64

struct RSPProcessCB {
    uint vertexStart;
    uint vertexCount;
    float prevFrameWeight;
    float curFrameWeight;
};

[[vk::push_constant]] ConstantBuffer<RSPProcessCB> gConstants : register(b0);
Buffer<int> srcPos : register(t1);
Buffer<int> srcVel : register(t2);
Buffer<float> srcTc : register(t3);
Buffer<uint> srcCol : register(t4);
Buffer<int> srcNorm : register(t5);
Buffer<uint> srcViewProjIndices : register(t6);
Buffer<uint> srcWorldIndices : register(t7);
Buffer<uint> srcFogIndices : register(t8);
Buffer<uint> srcLightIndices : register(t9);
Buffer<uint> srcLightCounts : register(t10);
Buffer<uint> srcLookAtIndices : register(t11);
StructuredBuffer<RSPViewport> rspViewportVector : register(t12);
StructuredBuffer<RSPFog> rspFogVector : register(t13);
StructuredBuffer<RSPLight> rspLightVector : register(t14);
StructuredBuffer<RSPLookAt> rspLookAtVector : register(t15);
StructuredBuffer<float4x4> viewProjTransforms : register(t16);
StructuredBuffer<float4x4> worldTransforms : register(t17);
RWStructuredBuffer<float4> dstPos : register(u18);
RWStructuredBuffer<float2> dstTc : register(u19);
RWStructuredBuffer<float4> dstCol : register(u20);

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint vertexIndex : SV_DispatchThreadID) {
    if (vertexIndex >= gConstants.vertexCount) {
        return;
    }

    const uint vertexOffsetIndex = gConstants.vertexStart + vertexIndex;
    const uint viewProjIndex = srcViewProjIndices[vertexOffsetIndex];
    const uint transformIndex = srcWorldIndices[vertexOffsetIndex];
    const float4x4 viewProjMat = viewProjTransforms[viewProjIndex];
    const float4x4 worldMat = worldTransforms[transformIndex];
    const uint posIndex = vertexOffsetIndex * 3;
    const uint normColIndex = vertexOffsetIndex * 4;
    const float3 norm = float3(
        srcNorm[normColIndex + 0] / 127.0f,
        srcNorm[normColIndex + 1] / 127.0f,
        srcNorm[normColIndex + 2] / 127.0f
    );

    // NDC Position.
    const float3 pos = float3(srcPos[posIndex + 0], srcPos[posIndex + 1], srcPos[posIndex + 2]);
    const float3 vel = float3(srcVel[posIndex + 0], srcVel[posIndex + 1], srcVel[posIndex + 2]);
    float4 tfPos = mul(mul(viewProjMat, worldMat), float4(pos - vel * (1.0f - gConstants.curFrameWeight), 1.0f));

    // Fog.
    const uint fogIndex = srcFogIndices[vertexOffsetIndex];
    float4 vertexColor;
    if (fogIndex > 0) {
        const RSPFog rspFog = rspFogVector[fogIndex - 1];
        const float fogAlpha = ((max(tfPos.z, 0) / tfPos.w) * rspFog.mul + rspFog.offset);
        vertexColor.a = clamp(fogAlpha / 255.0f, 0.0f, 1.0f);
    }
    else {
        vertexColor.a = srcCol[normColIndex + 3] / 255.0f;
    }

    // Texgen.
    const uint tcIndex = vertexOffsetIndex * 2;
    const uint lookAtIndex = srcLookAtIndices[vertexOffsetIndex];
    float2 tc = float2(srcTc[tcIndex + 0], srcTc[tcIndex + 1]);
    if (lookAtIndex & RSP_LOOKAT_INDEX_ENABLED) {
        const bool textureGenLinear = lookAtIndex & RSP_LOOKAT_INDEX_LINEAR;
        const uint extractedIndex = (lookAtIndex >> RSP_LOOKAT_INDEX_SHIFT);
        const RSPLookAt lookAt = rspLookAtVector[extractedIndex];
        tc = computeTextureGen(tc, norm, lookAt, textureGenLinear, worldMat);
    }

    // Lighting.
    const uint lightIndex = srcLightIndices[vertexOffsetIndex];
    const uint lightCount = srcLightCounts[vertexOffsetIndex];
    if (lightCount > 0) {
        const uint ambientIndex = lightIndex + lightCount - 1;
        float3 resultColor = rspLightVector[ambientIndex].col;
        for (uint i = lightIndex; i < ambientIndex; i++) {
            const RSPLight light = rspLightVector[i];
            if (light.kc > 0) {
                resultColor += computePosLight(pos, norm, light, worldMat);
            }
            else {
                resultColor += computeDirLight(norm, light, worldMat);
            }
        }

        vertexColor.rgb = min(resultColor, 1.0f);
    }
    else {
        vertexColor.rgb = float3(
            srcCol[normColIndex + 0] / 255.0f, 
            srcCol[normColIndex + 1] / 255.0f, 
            srcCol[normColIndex + 2] / 255.0f
        );
    }
    
    // HACK: For handling geometry exactly at the near clip plane of the viewport. This hack can
    // probably be removed once the behavior of the RSP has been reviewed in this case.
    if (tfPos.w == 0.0f) {
        tfPos.w = 1e-6f;
    }

    // Convert to N64 screen position.
    const RSPViewport rspViewport = rspViewportVector[viewProjIndex];
    const float3 ndcPos = tfPos.xyz / float3(tfPos.w, -tfPos.w, tfPos.w);
    const float4 screenPos = float4(ndcPos * rspViewport.scale + rspViewport.translate, tfPos.w);
    dstPos[vertexOffsetIndex] = screenPos;
    dstTc[vertexOffsetIndex] = tc;
    dstCol[vertexOffsetIndex] = vertexColor;
}