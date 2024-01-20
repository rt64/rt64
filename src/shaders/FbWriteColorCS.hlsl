//
// RT64
//

#include "FbCommon.hlsli"

[[vk::push_constant]] ConstantBuffer<FbCommonCB> gConstants : register(b0, space0);
RWBuffer<uint> gOutput : register(u1, space0);
Texture2D<float4> gInput : register(t0, space1);

[numthreads(FB_COMMON_WORKGROUP_SIZE, FB_COMMON_WORKGROUP_SIZE, 1)]
void CSMain(uint2 coord : SV_DispatchThreadID) {
    if ((coord.x < gConstants.resolution.x) && (coord.y < gConstants.resolution.y)) {
        uint2 offsetCoord = gConstants.offset + coord;
        uint dstIndex = offsetCoord.y * gConstants.resolution.x + offsetCoord.x;
        float4 color = gInput.Load(uint3(offsetCoord, 0));
        bool oddColumn = (offsetCoord.x & 1);
        uint nativeUint = Float4ToUINT(color, gConstants.siz, gConstants.fmt, oddColumn);
        gOutput[dstIndex] = EndianSwapUINT(nativeUint, gConstants.siz);
    }
}