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

ConstantBuffer<RaytracingParams> RtParams : register(b25, space0);
RaytracingAccelerationStructure SceneBVH : register(t26, space0);
ByteAddressBuffer posBuffer : register(t27, space0);
ByteAddressBuffer normBuffer : register(t28, space0);
ByteAddressBuffer velBuffer : register(t29, space0);
ByteAddressBuffer genTexCoordBuffer : register(t30, space0);
ByteAddressBuffer shadedColBuffer : register(t31, space0);
ByteAddressBuffer srcFogIndices : register(t32, space0);
ByteAddressBuffer srcLightIndices : register(t33, space0);
ByteAddressBuffer srcLightCounts : register(t34, space0);
ByteAddressBuffer indexBuffer : register(t35, space0);
StructuredBuffer<RSPFog> RSPFogVector : register(t36, space0);
StructuredBuffer<RSPLight> RSPLightVector : register(t37, space0);
StructuredBuffer<ExtraParams> instanceExtraParams : register(t38, space0);
StructuredBuffer<PointLight> SceneLights : register(t39, space0);
StructuredBuffer<InterleavedRaster> interleavedRasters : register(t40, space0);
RWBuffer<float4> gHitVelocityDistance : register(u41, space0);
RWBuffer<float4> gHitColor : register(u42, space0);
RWBuffer<float4> gHitNormalFog : register(u43, space0);
RWBuffer<uint> gHitInstanceId : register(u44, space0);
RWTexture2D<float4> gViewDirection : register(u45, space0);
RWTexture2D<float4> gShadingPosition : register(u46, space0);
RWTexture2D<float4> gShadingNormal : register(u47, space0);
RWTexture2D<float4> gShadingSpecular : register(u48, space0);
RWTexture2D<float4> gDiffuse : register(u49, space0);
RWTexture2D<int> gInstanceId : register(u50, space0);
RWTexture2D<float4> gDirectLightAccum : register(u51, space0);
RWTexture2D<float4> gIndirectLightAccum : register(u52, space0);
RWTexture2D<float4> gReflection : register(u53, space0);
RWTexture2D<float4> gRefraction : register(u54, space0);
RWTexture2D<float4> gTransparent : register(u55, space0);
RWTexture2D<float2> gFlow : register(u56, space0);
RWTexture2D<float> gReactiveMask : register(u57, space0);
RWTexture2D<float> gLockMask : register(u58, space0);
RWTexture2D<float4> gNormalRoughness : register(u59, space0);
RWTexture2D<float> gDepth : register(u60, space0);
RWTexture2D<float4> gPrevNormalRoughness : register(u61, space0);
RWTexture2D<float> gPrevDepth : register(u62, space0);
RWTexture2D<float4> gPrevDirectLightAccum : register(u63, space0);
RWTexture2D<float4> gPrevIndirectLightAccum : register(u64, space0);
RWTexture2D<float4> gFilteredDirectLight : register(u65, space0);
RWTexture2D<float4> gFilteredIndirectLight : register(u66, space0);
Texture2D<float4> gBlueNoise : register(t67, space0);

// Set 3 - Framebuffer.
Texture2D<float4> gBackgroundColor : register(t1, space3);