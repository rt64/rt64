//
// Im3d Dx12
//

#include "Im3DCommon.hlsli"
#include "FbRendererRT.hlsli"

float4 PSMain(VS_OUTPUT _in) : SV_Target {
    uint2 depthPos = (_in.m_position.xy / RtParams.resolution.zw) * RtParams.resolution.xy;
    float bufferDepth = gDepth[depthPos].r;
    float4 projPos = mul(RtParams.viewProj, float4(_in.m_worldPosition, 1.0f));
    float pixelDepth = projPos.z / projPos.w;
    float4 ret = _in.m_color;

    // Dither and make the pixels more transparent if occluded.
    if (bufferDepth < pixelDepth) {
        ret *= 0.5f;
        clip(fmod(_in.m_position.x + _in.m_position.y, 2.0f) - 1.0f);
    }
    
    return ret;
}