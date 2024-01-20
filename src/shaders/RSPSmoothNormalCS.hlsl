//
// RT64
//
// Computes the smooth normal of the vertices by welding the vertices with similar position and color.
//

#define GROUP_SIZE 64

struct RSPSmoothNormalCB {
    uint indexStart;
    uint indexCount;
};

[[vk::push_constant]] ConstantBuffer<RSPSmoothNormalCB> gConstants : register(b0);
StructuredBuffer<float4> srcWorldPos : register(t1);
StructuredBuffer<uint> srcCol : register(t2);
StructuredBuffer<uint> srcFaceIndices : register(t3);
RWStructuredBuffer<float4> dstWorldNorm : register(u4);

float3 computeSmoothNormal(uint vertexIndex) {
    const float PosDistSqr = 1.0f;
    const float3 vertexPos = srcWorldPos[vertexIndex].xyz;
    const uint vertexCol = srcCol[vertexIndex];
    float3 vertexNorm = float3(0.0f, 0.0f, 0.0f);
    const uint triangleCount = gConstants.indexCount / 3;
    for (uint t = 0; t < triangleCount; t++) {
        for (uint j = 0; j < 3; j++) {
            const uint cmpIndex = srcFaceIndices[gConstants.indexStart + t * 3 + j];
            const float3 cmpPos = srcWorldPos[cmpIndex].xyz;
            const uint cmpCol = srcCol[cmpIndex];
            const float3 posDelta = cmpPos - vertexPos;
            if ((dot(posDelta, posDelta) <= PosDistSqr) && (cmpCol == vertexCol)) {
                const uint indexA = srcFaceIndices[gConstants.indexStart + t * 3 + 0];
                const uint indexB = srcFaceIndices[gConstants.indexStart + t * 3 + 1];
                const uint indexC = srcFaceIndices[gConstants.indexStart + t * 3 + 2];
                const float3 triA = srcWorldPos[indexA].xyz;
                const float3 triB = srcWorldPos[indexB].xyz;
                const float3 triC = srcWorldPos[indexC].xyz;
                vertexNorm += normalize(cross(triB - triA, triC - triA));
            }
        }
    }

    return normalize(vertexNorm);
}

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint triangleIndex : SV_DispatchThreadID) {
    if ((triangleIndex * 3) >= gConstants.indexCount) {
        return;
    }

    const uint baseIndex = gConstants.indexStart + triangleIndex * 3;
    const uint v0 = srcFaceIndices[baseIndex + 0];
    const uint v1 = srcFaceIndices[baseIndex + 1];
    const uint v2 = srcFaceIndices[baseIndex + 2];
    const float3 n0 = computeSmoothNormal(v0);
    const float3 n1 = computeSmoothNormal(v1);
    const float3 n2 = computeSmoothNormal(v2);
    AllMemoryBarrierWithGroupSync();
    dstWorldNorm[v0] = float4(n0, 1.0f);
    dstWorldNorm[v1] = float4(n1, 1.0f);
    dstWorldNorm[v2] = float4(n2, 1.0f);
}