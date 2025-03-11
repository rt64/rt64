//
// RT64
//

struct Constants {
    float4 colorAdd;
    uint textureIndex;
};

[[vk::push_constant]] ConstantBuffer<Constants> gConstants : register(b0);

float4 PSMain(in float4 pos : SV_Position, in float2 uv : TEXCOORD0) : SV_TARGET {
    float4 gradientColor = float4(uv, 1.0f, 0.5f);
    
    // Check if colorAdd has any non-zero components
    if (length(gConstants.colorAdd) > 0.001f) {
        return gConstants.colorAdd;
    }
    
    return gradientColor;
}