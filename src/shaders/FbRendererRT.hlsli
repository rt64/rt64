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

ConstantBuffer<RaytracingParams> RtParams : register(b16, space0);
RaytracingAccelerationStructure SceneBVH : register(t17, space0);
ByteAddressBuffer posBuffer : register(t18, space0);
ByteAddressBuffer normBuffer : register(t19, space0);
ByteAddressBuffer velBuffer : register(t20, space0);
ByteAddressBuffer genTexCoordBuffer : register(t21, space0);
ByteAddressBuffer shadedColBuffer : register(t22, space0);
ByteAddressBuffer srcFogIndices : register(t23, space0);
ByteAddressBuffer srcLightIndices : register(t24, space0);
ByteAddressBuffer srcLightCounts : register(t25, space0);
ByteAddressBuffer indexBuffer : register(t26, space0);
StructuredBuffer<RSPFog> RSPFogVector : register(t27, space0);
StructuredBuffer<RSPLight> RSPLightVector : register(t28, space0);
StructuredBuffer<ExtraParams> instanceExtraParams : register(t29, space0);
StructuredBuffer<PointLight> SceneLights : register(t30, space0);
StructuredBuffer<InterleavedRaster> interleavedRasters : register(t31, space0);
RWBuffer<float4> gHitVelocityDistance : register(u32, space0);
RWBuffer<float4> gHitColor : register(u33, space0);
RWBuffer<float4> gHitNormalFog : register(u34, space0);
RWBuffer<uint> gHitInstanceId : register(u35, space0);
RWTexture2D<float4> gViewDirection : register(u36, space0);
RWTexture2D<float4> gShadingPosition : register(u37, space0);
RWTexture2D<float4> gShadingNormal : register(u38, space0);
RWTexture2D<float4> gShadingSpecular : register(u39, space0);
RWTexture2D<float4> gDiffuse : register(u40, space0);
RWTexture2D<int> gInstanceId : register(u41, space0);
RWTexture2D<float4> gDirectLightAccum : register(u42, space0);
RWTexture2D<float4> gIndirectLightAccum : register(u43, space0);
RWTexture2D<float4> gReflection : register(u44, space0);
RWTexture2D<float4> gRefraction : register(u45, space0);
RWTexture2D<float4> gTransparent : register(u46, space0);
RWTexture2D<float2> gFlow : register(u47, space0);
RWTexture2D<float> gReactiveMask : register(u48, space0);
RWTexture2D<float> gLockMask : register(u49, space0);
RWTexture2D<float4> gNormalRoughness : register(u50, space0);
RWTexture2D<float> gDepth : register(u51, space0);
RWTexture2D<float4> gPrevNormalRoughness : register(u52, space0);
RWTexture2D<float> gPrevDepth : register(u53, space0);
RWTexture2D<float4> gPrevDirectLightAccum : register(u54, space0);
RWTexture2D<float4> gPrevIndirectLightAccum : register(u55, space0);
RWTexture2D<float4> gFilteredDirectLight : register(u56, space0);
RWTexture2D<float4> gFilteredIndirectLight : register(u57, space0);
Texture2D<float4> gBlueNoise : register(t58, space0);

// Set 3 - Framebuffer.
Texture2D<float4> gBackgroundColor : register(t1, space3);