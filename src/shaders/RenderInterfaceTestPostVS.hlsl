//
// RT64
//

void VSMain(in uint id : SV_VertexID, out float4 pos : SV_Position, out float2 uv : TEXCOORD0) {
    uv.x = (id == 2) ? 2.0f : 0.0f;
    uv.y = (id == 1) ? 2.0f : 0.0f;
    pos = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 1.0f, 1.0f);
}