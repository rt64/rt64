//
// RT64
//

float4 PSMain(in float2 uv : TEXCOORD) : SV_TARGET {
    return float4(uv, 1.0f, 1.0f);
}