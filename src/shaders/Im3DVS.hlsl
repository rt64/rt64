//
// Im3d Dx12
//

#include "Im3DCommon.hlsli"
#include "FbRendererRT.hlsli"

struct VS_INPUT {
    float4 m_positionSize : POSITION_SIZE;
    float4 m_color        : COLOR;
};

VS_OUTPUT VSMain(VS_INPUT _in) {
    VS_OUTPUT ret;
    ret.m_color = _in.m_color.abgr;
    ret.m_size = _in.m_positionSize.w;
    ret.m_position = mul(mul(RtParams.projection, RtParams.view), float4(_in.m_positionSize.xyz, 1.0));
    ret.m_worldPosition = _in.m_positionSize.xyz;
    return ret;
}