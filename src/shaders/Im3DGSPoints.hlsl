//
// Im3d Dx12
//

#include "Im3DCommon.hlsli"
#include "FbRendererRT.hlsli"

// expand point -> triangle strip (quad)
[maxvertexcount(4)]
void GSMain(point VS_OUTPUT _in[1], inout TriangleStream<VS_OUTPUT> out_) {
    VS_OUTPUT ret;

    float2 scale = 1.0 / RtParams.resolution.zw * _in[0].m_size;
    ret.m_size = _in[0].m_size;
    ret.m_color = _in[0].m_color;
    ret.m_worldPosition = _in[0].m_worldPosition;

    ret.m_position = float4(_in[0].m_position.xy + float2(-1.0, -1.0) * scale * _in[0].m_position.w, _in[0].m_position.zw);
    out_.Append(ret);

    ret.m_position = float4(_in[0].m_position.xy + float2(1.0, -1.0) * scale * _in[0].m_position.w, _in[0].m_position.zw);
    out_.Append(ret);

    ret.m_position = float4(_in[0].m_position.xy + float2(-1.0, 1.0) * scale * _in[0].m_position.w, _in[0].m_position.zw);
    out_.Append(ret);

    ret.m_position = float4(_in[0].m_position.xy + float2(1.0, 1.0) * scale * _in[0].m_position.w, _in[0].m_position.zw);
    out_.Append(ret);
}