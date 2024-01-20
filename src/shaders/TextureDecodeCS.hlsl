//
// RT64
//

#include "TextureDecoder.hlsli"

#define GROUP_SIZE 8

struct TextureDecodeCB {
    uint2 Resolution;
    uint fmt;
    uint siz;
    uint address;
    uint stride;
    uint tlut;
    uint palette;
};

[[vk::push_constant]] ConstantBuffer<TextureDecodeCB> gConstants : register(b0);
Texture1D<uint> TMEM : register(t1);
RWTexture2D<float4> RGBA32 : register(u2);

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void CSMain(uint2 coord : SV_DispatchThreadID) {
    if ((coord.x < gConstants.Resolution.x) && (coord.y < gConstants.Resolution.y)) {
        RGBA32[coord] = sampleTMEM(coord, gConstants.siz, gConstants.fmt, gConstants.address, gConstants.stride, gConstants.tlut, gConstants.palette, TMEM);
    }
}