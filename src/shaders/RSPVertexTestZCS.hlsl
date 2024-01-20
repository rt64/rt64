//
// RT64
//

#include "shared/rt64_rsp_vertex_test_z.h"

[[vk::push_constant]] ConstantBuffer<RSPVertexTestZCB> gConstants : register(b0);
StructuredBuffer<float4> screenPos : register(t1);
StructuredBuffer<uint> srcFaceIndices : register(t2);
RWStructuredBuffer<uint> dstFaceIndices : register(u3);

#if defined(MULTISAMPLING)
Texture2DMS<float> gBackgroundDepth : register(t2, space1);

float sampleBackgroundDepth(int2 pixelPos, uint sampleIndex) {
    return gBackgroundDepth.Load(pixelPos, sampleIndex);
}
#else
Texture2D<float> gBackgroundDepth : register(t2, space1);

float sampleBackgroundDepth(int2 pixelPos, uint sampleIndex) {
    return gBackgroundDepth.Load(int3(pixelPos, 0));
}
#endif

[numthreads(1, 1, 1)]
void CSMain() {
    float4 pixelPos = screenPos[gConstants.vertexIndex];
    int2 pixelPosInt = pixelPos.xy * gConstants.resolutionScale;
    float pixelDepth = sampleBackgroundDepth(pixelPosInt, 0);
    if (pixelDepth <= pixelPos.z) {
        for (uint i = 0; i < gConstants.indexCount; i++) {
            dstFaceIndices[gConstants.dstIndexStart + i] = gConstants.vertexIndex;
        }
    }
    else {
        for (uint i = 0; i < gConstants.indexCount; i++) {
            dstFaceIndices[gConstants.dstIndexStart + i] = srcFaceIndices[gConstants.srcIndexStart + i];
        }
    }
}