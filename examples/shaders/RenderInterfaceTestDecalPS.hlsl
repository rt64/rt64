//
// RT64
//

Texture2DMS<float> gDepth : register(t0);

float4 PSMain(in float4 pos : SV_Position, in uint sampleIndex : SV_SampleIndex, in float2 uv : TEXCOORD0) : SV_TARGET {
    float depth = gDepth.Load(floor(pos.xy), sampleIndex);
    if (abs(pos.z - depth) > 1e-6f) {
        discard;
    }
    
    return float4(1.0f, uv, 1.0f);
}