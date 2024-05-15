//
// RT64
//

struct Constants {
    float4 Multiply;
    uint2 Resolution;
};

[[vk::push_constant]] ConstantBuffer<Constants> gConstants : register(b0);

Texture2D<float4> gBlueNoiseTexture : register(t1);
SamplerState gSampler : register(s2);
[[vk::image_format("rgba8")]]
RWTexture2D<float4> gTarget : register(u16, space1);

[numthreads(8, 8, 1)]
void CSMain(uint2 coord : SV_DispatchThreadID) {
    if (any(coord >= gConstants.Resolution)) {
        return;
    }
    
    float2 blueNoiseUV = float2(coord) / float2(gConstants.Resolution);
    float4 blueNoise = float4(gBlueNoiseTexture.SampleLevel(gSampler, blueNoiseUV, 0).rgb, 1.0f);
    gTarget[coord] *= (blueNoise * gConstants.Multiply);
}