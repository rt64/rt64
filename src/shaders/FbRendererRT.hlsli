//
// RT64
//

#pragma once

#include "shared/rt64_extra_params.h"
#include "shared/rt64_interleaved_raster.h"
#include "shared/rt64_point_light.h"
#include "shared/rt64_raytracing_params.h"
#include "shared/rt64_rsp_fog.h"
#include "shared/rt64_rsp_light.h"

ConstantBuffer<RaytracingParams> RtParams : register(b7, space0);
RaytracingAccelerationStructure SceneBVH : register(t8, space0);
ByteAddressBuffer posBuffer : register(t10, space0);
ByteAddressBuffer normBuffer : register(t11, space0);
ByteAddressBuffer velBuffer : register(t12, space0);
ByteAddressBuffer genTexCoordBuffer : register(t13, space0);
ByteAddressBuffer shadedColBuffer : register(t14, space0);
ByteAddressBuffer srcFogIndices : register(t15, space0);
ByteAddressBuffer srcLightIndices : register(t16, space0);
ByteAddressBuffer srcLightCounts : register(t17, space0);
ByteAddressBuffer indexBuffer : register(t18, space0);
StructuredBuffer<RSPFog> RSPFogVector : register(t19, space0);
StructuredBuffer<RSPLight> RSPLightVector : register(t20, space0);
StructuredBuffer<ExtraParams> instanceExtraParams : register(t21, space0);
StructuredBuffer<PointLight> SceneLights : register(t22, space0);
StructuredBuffer<InterleavedRaster> interleavedRasters : register(t23, space0);
RWBuffer<float4> gHitVelocityDistance : register(u24, space0);
RWBuffer<float4> gHitColor : register(u25, space0);
RWBuffer<float4> gHitNormalFog : register(u26, space0);
RWBuffer<uint> gHitInstanceId : register(u27, space0);
RWTexture2D<float4> gViewDirection : register(u28, space0);
RWTexture2D<float4> gShadingPosition : register(u29, space0);
RWTexture2D<float4> gShadingNormal : register(u30, space0);
RWTexture2D<float4> gShadingSpecular : register(u31, space0);
RWTexture2D<float4> gDiffuse : register(u32, space0);
RWTexture2D<int> gInstanceId : register(u33, space0);
RWTexture2D<float4> gDirectLightAccum : register(u34, space0);
RWTexture2D<float4> gIndirectLightAccum : register(u35, space0);
RWTexture2D<float4> gReflection : register(u36, space0);
RWTexture2D<float4> gRefraction : register(u37, space0);
RWTexture2D<float4> gTransparent : register(u38, space0);
RWTexture2D<float2> gFlow : register(u39, space0);
RWTexture2D<float> gReactiveMask : register(u40, space0);
RWTexture2D<float> gLockMask : register(u41, space0);
RWTexture2D<float4> gNormal : register(u42, space0);
RWTexture2D<float> gDepth : register(u43, space0);
RWTexture2D<float4> gPrevNormal : register(u44, space0);
RWTexture2D<float> gPrevDepth : register(u45, space0);
RWTexture2D<float4> gPrevDirectLightAccum : register(u46, space0);
RWTexture2D<float4> gPrevIndirectLightAccum : register(u47, space0);
RWTexture2D<float4> gFilteredDirectLight : register(u48, space0);
RWTexture2D<float4> gFilteredIndirectLight : register(u49, space0);
SamplerState gBackgroundClampSampler : register(s50, space0);
SamplerState gBackgroundMirrorSampler : register(s51, space0);
Texture2D<float4> gBlueNoise : register(t52, space0);

// Set 3 - Framebuffer.
Texture2D<float4> gBackgroundColor : register(t1, space3);