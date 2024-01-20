//
// RT64
//

#define BLOCK_SIZE 8

struct BoxFilterCB {
    int2 Resolution;
    int2 ResolutionScale;
    int2 Misalignment;
};

[[vk::push_constant]] ConstantBuffer<BoxFilterCB> gConstants : register(b0);
Texture2D<float4> gInput : register(t1);
RWTexture2D<float4> gOutput : register(u2);

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void CSMain(uint2 coord : SV_DispatchThreadID) {
    float4 resultColor = 0.0f;
    int2 maxCoord = gConstants.Resolution - int2(1, 1);
    for (int x = 0; x < gConstants.ResolutionScale.x; x++) {
        for (int y = 0; y < gConstants.ResolutionScale.y; y++) {
            int2 clampedCoord = clamp(coord * gConstants.ResolutionScale + int2(x, y) + gConstants.Misalignment, int2(0, 0), maxCoord);
            resultColor += gInput.Load(int3(clampedCoord, 0));
        }
    }

    gOutput[coord] = resultColor / (gConstants.ResolutionScale.x * gConstants.ResolutionScale.y);
}