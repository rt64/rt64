//
// RT64
//

void VSMain(in float2 vertexPos : POSITION, in float2 vertexUV : TEXCOORD, out float4 pos : SV_POSITION, out float2 uv : TEXCOORD) {
    pos = float4(vertexPos, 1.0f, 1.0f);
    uv = vertexUV;
}