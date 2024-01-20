//
// Im3d Dx12
//

#include "Im3DCommon.hlsli"
#include "FbRendererRT.hlsli"

[maxvertexcount(4)]
void GSMain(line VS_OUTPUT _in[2], inout TriangleStream<VS_OUTPUT> out_) {
    float2 pos0 = _in[0].m_position.xy / _in[0].m_position.w;
    float2 pos1 = _in[1].m_position.xy / _in[1].m_position.w;
    
    float2 dir = pos0 - pos1;
    dir = normalize(float2(dir.x, dir.y * RtParams.resolution.w / RtParams.resolution.z)); // correct for aspect ratio
    float2 tng0 = float2(-dir.y, dir.x);
    float2 tng1 = tng0 * _in[1].m_size / RtParams.resolution.zw;
    tng0 = tng0 * _in[0].m_size / RtParams.resolution.zw;

    VS_OUTPUT ret;
    
    // line start
    ret.m_size = _in[0].m_size;
    ret.m_color = _in[0].m_color;
    ret.m_position = float4((pos0 - tng0) * _in[0].m_position.w, _in[0].m_position.zw);
    ret.m_worldPosition = _in[0].m_worldPosition;
    out_.Append(ret);
    ret.m_position = float4((pos0 + tng0) * _in[0].m_position.w, _in[0].m_position.zw);
    out_.Append(ret);

    // line end
    ret.m_size = _in[1].m_size;
    ret.m_color = _in[1].m_color;
    ret.m_position = float4((pos1 - tng1) * _in[1].m_position.w, _in[1].m_position.zw);
    ret.m_worldPosition = _in[1].m_worldPosition;
    out_.Append(ret);
    ret.m_position = float4((pos1 + tng1) * _in[1].m_position.w, _in[1].m_position.zw);
    out_.Append(ret);
}