//
// RT64
//

float4 PSMain(in float4 pos : SV_Position, in float2 uv : TEXCOORD0) : SV_TARGET {
    return float4(uv, 1.0f, 0.5f);
}