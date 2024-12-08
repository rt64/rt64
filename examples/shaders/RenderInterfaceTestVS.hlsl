//
// RT64
//

void VSMain(in float2 vertexPos : POSITION, in float2 vertexUV : TEXCOORD0, out float4 pos : SV_POSITION, out float2 uv : TEXCOORD0) {
    pos = float4(vertexPos, 0.5f, 1.0f);
    uv = vertexUV;
}