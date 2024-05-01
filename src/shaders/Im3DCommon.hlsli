//
// Im3d Dx12
//

struct VS_OUTPUT {
    linear        float4 m_position      : SV_POSITION;
    linear        float3 m_worldPosition : POSITION;
    linear        float4 m_color         : COLOR;
    noperspective float  m_size          : SIZE;
};