//
// RT64
//

#define GROUP_SIZE 64

struct RSPWorldCB {
    uint vertexStart;
    uint vertexCount;
    float prevFrameWeight;
    float curFrameWeight;
};

[[vk::push_constant]] ConstantBuffer<RSPWorldCB> gConstants : register(b0);
Buffer<int> srcPos : register(t1);
Buffer<int> srcVel : register(t2);
Buffer<int> srcNorm : register(t3);
Buffer<uint> srcIndices : register(t4);
StructuredBuffer<float4x4> worldMats : register(t5);
StructuredBuffer<float4x4> invTWorldMats : register(t6);
StructuredBuffer<float4x4> prevWorldMats : register(t7);
RWStructuredBuffer<float4> dstPos : register(u8);
RWStructuredBuffer<float4> dstNorm : register(u9);
RWStructuredBuffer<float4> dstVel : register(u10);

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint vertexIndex : SV_DispatchThreadID) {
    if (vertexIndex >= gConstants.vertexCount) {
        return;
    }
    
    const uint vertexOffsetIndex = gConstants.vertexStart + vertexIndex;
    const uint transformIndex = srcIndices[vertexOffsetIndex];
    const uint posIndex = vertexOffsetIndex * 3;
    const uint normIndex = vertexOffsetIndex * 4;
    const float3 pos = float3(srcPos[posIndex + 0], srcPos[posIndex + 1], srcPos[posIndex + 2]);
    const float3 vel = float3(srcVel[posIndex + 0], srcVel[posIndex + 1], srcVel[posIndex + 2]);
    const float3 norm = float3(srcNorm[normIndex + 0], srcNorm[normIndex + 1], srcNorm[normIndex + 2]);
    const float4 worldPos = mul(worldMats[transformIndex], float4(pos - vel * (1.0f - gConstants.curFrameWeight), 1.0f));
    const float4 worldNorm = all(norm == 0.0f) ? float4(0.0f, 0.0f, 0.0f, 1.0f) : float4(normalize(mul(invTWorldMats[transformIndex], float4(norm, 0.0f)).xyz), 1.0f);
    const float4 prevWorldPos = mul(prevWorldMats[transformIndex], float4(pos - vel * (1.0f - gConstants.prevFrameWeight), 1.0f));
    dstPos[vertexOffsetIndex] = worldPos;
    dstNorm[vertexOffsetIndex] = worldNorm;
    dstVel[vertexOffsetIndex] = worldPos - prevWorldPos;
}