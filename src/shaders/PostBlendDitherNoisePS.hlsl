//
// RT64
//

#include "shared/rt64_raster_params.h"

#include "FbRendererCommon.hlsli"
#include "Random.hlsli"

[[vk::push_constant]] ConstantBuffer<RasterParams> gConstants : register(b0, space0);

void PSMain(
      in float4 vertexPosition : SV_POSITION
    , in float2 vertexUV : TEXCOORD
    , in float4 vertexSmoothColor : COLOR0
    , nointerpolation in float4 vertexFlatColor : COLOR1
    , [[vk::location(0)]] [[vk::index(0)]] out float4 resultColor : SV_TARGET0
)
{
    int2 pixelPosSeed = floor(vertexPosition.xy);
    uint randomSeed = initRand(FrParams.frameCount, gConstants.renderIndex * (pixelPosSeed.y * 65536 + pixelPosSeed.x), 16);
    const float Range = (7.0f * FrParams.ditherNoiseStrength) / 255.0f;
    const float HalfRange = Range / 2.0f;
    resultColor.r = nextRand(randomSeed) * Range - HalfRange;
    resultColor.g = nextRand(randomSeed) * Range - HalfRange;
    resultColor.b = nextRand(randomSeed) * Range - HalfRange;
    resultColor.a = 0.0f;
    
#if defined(ADD_MODE)
    resultColor = max(resultColor, 0.0f);
#elif defined(SUB_MODE)
    resultColor = max(-resultColor, 0.0f);
#endif
}