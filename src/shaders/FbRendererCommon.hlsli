//
// RT64
//

#pragma once

#include "shared/rt64_fb_common.h"
#include "shared/rt64_frame_params.h"
#include "shared/rt64_framebuffer_params.h"
#include "shared/rt64_gpu_tile.h"
#include "shared/rt64_rdp_params.h"
#include "shared/rt64_rdp_tile.h"
#include "shared/rt64_render_indices.h"
#include "shared/rt64_render_params.h"

// Set 0 - Global resources and Enhancements (Raytracing or other).
ConstantBuffer<FrameParams> FrParams : register(b1, space0);
StructuredBuffer<RDPParams> instanceRDPParams : register(t2, space0);
StructuredBuffer<RDPTile> RDPTiles : register(t3, space0);
StructuredBuffer<GPUTile> GPUTiles : register(t4, space0);
StructuredBuffer<RenderIndices> instanceRenderIndices : register(t5, space0);
StructuredBuffer<RenderParams> DynamicRenderParams : register(t6, space0);
SamplerState gWrapWrapSampler : register(s7, space0);
SamplerState gWrapMirrorSampler : register(s8, space0);
SamplerState gWrapClampSampler : register(s9, space0);
SamplerState gMirrorWrapSampler : register(s10, space0);
SamplerState gMirrorMirrorSampler : register(s11, space0);
SamplerState gMirrorClampSampler : register(s12, space0);
SamplerState gClampWrapSampler : register(s13, space0);
SamplerState gClampMirrorSampler : register(s14, space0);
SamplerState gClampClampSampler : register(s15, space0);

// Set 1 - RGBA32 texture cache.
Texture2D<float4> gTextures[] : register(t0, space1);

// Set 2 - TMEM texture cache.
Texture1D<uint> gTMEM[] : register(t0, space2);

// Set 3 - Framebuffer.
ConstantBuffer<FramebufferParams> FbParams : register(b0, space3);