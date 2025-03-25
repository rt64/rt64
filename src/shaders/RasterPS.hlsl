//
// RT64
//

#include "shared/rt64_blender.h"
#include "shared/rt64_color_combiner.h"
#include "shared/rt64_raster_params.h"

#include "Depth.hlsli"
#include "FbRendererCommon.hlsli"
#include "Library.hlsli"
#include "Random.hlsli"
#include "TextureSampler.hlsli"

[[vk::push_constant]] ConstantBuffer<RasterParams> gConstants : register(b0, space0);

#if defined(MULTISAMPLING)
Texture2DMS<float> gBackgroundDepth : register(t2, space3);

float sampleBackgroundDepth(int2 pixelPos, uint sampleCount) {
    float v = gBackgroundDepth.Load(pixelPos, 0);
    if (sampleCount >= 2) {
        v += gBackgroundDepth.Load(pixelPos, 1);
    }

    if (sampleCount >= 4) {
        v += gBackgroundDepth.Load(pixelPos, 2);
        v += gBackgroundDepth.Load(pixelPos, 3);
    }

    if (sampleCount >= 8) {
        v += gBackgroundDepth.Load(pixelPos, 4);
        v += gBackgroundDepth.Load(pixelPos, 5);
        v += gBackgroundDepth.Load(pixelPos, 6);
        v += gBackgroundDepth.Load(pixelPos, 7);
    }

    return v / float(sampleCount);
}
#elif defined(DYNAMIC_RENDER_PARAMS) || defined(SPEC_CONSTANT_RENDER_PARAMS) || defined(LIBRARY)
Texture2D<float> gBackgroundDepth : register(t2, space3);

float sampleBackgroundDepth(int2 pixelPos, uint sampleCount) {
    return gBackgroundDepth.Load(int3(pixelPos, 0));
}
#endif

LIBRARY_EXPORT bool RasterPS(const RenderParams rp, float4 vertexPosition, float2 vertexUV, float4 vertexSmoothColor, float4 vertexFlatColor,
    bool isFrontFace, out float4 resultColor, out float4 resultAlpha) 
{
    const OtherMode otherMode = { rp.omL, rp.omH };
#if defined(DYNAMIC_RENDER_PARAMS)
    if ((otherMode.cycleType() != G_CYC_COPY) && renderFlagCulling(rp.flags) && isFrontFace) {
        return false;
    }
#endif
    
    const uint instanceIndex = instanceRenderIndices[gConstants.renderIndex].instanceIndex;
    const float4 vertexColor = renderFlagSmoothShade(rp.flags) ? vertexSmoothColor : float4(vertexFlatColor.rgb, vertexSmoothColor.a);
    const ColorCombiner colorCombiner = { rp.ccL, rp.ccH };
    const bool depthClampNear = renderFlagNoN(rp.flags);
    const bool depthDecal = (otherMode.zMode() == ZMODE_DEC);
    const bool zSourcePrim = (otherMode.zSource() == G_ZS_PRIM);
    int2 pixelPosSeed = floor(vertexPosition.xy);
    uint randomSeed = initRand(FrParams.frameCount, instanceIndex * pixelPosSeed.y * pixelPosSeed.x, 16); // TODO: Review seed.

    // Handle no-nearclipping by clamping the minimum depth and manually clipping above the maximum.
    if (depthClampNear) {
        // Since depth clip is disabled on the PSO so near clip can be ignored, we manually clip any values above the allowed depth.
        if (vertexPosition.z > 1.0f) {
            return false;
        }
    }
#ifdef DYNAMIC_RENDER_PARAMS
    // We emulate depth clip on the dynamic version of the shader.
    else {
        if ((vertexPosition.z < 0.0f) || (vertexPosition.z > 1.0f)) {
            return false;
        }
    }
#endif

    if (depthDecal) {
        // Sample the depth buffer for this pixel to compare for the decal check.
        int2 pixelPos = floor(vertexPosition.xy);
        uint sampleCount = 1U << renderFlagSampleCount(rp.flags);
        float surfaceDepth = sampleBackgroundDepth(pixelPos, sampleCount);

        // Calculate the decal depth tolerance based on the depth derivatives (or the prim dz value if prim depth source is enabled).
        float dz;
        if (zSourcePrim) {
            dz = instanceRDPParams[instanceIndex].primDepth.y;
        }
        else {
            dz = (abs(ddx(vertexPosition.z)) + abs(ddy(vertexPosition.z))) * FbParams.resolutionScale.y;
        }

        // Perform the decal depth tolerance check.
        const float DepthTolerance = max(CoplanarDepthTolerance(surfaceDepth), dz);
        const float pixelDepth = select(depthClampNear, max(vertexPosition.z, 0.0f), vertexPosition.z);
        if (abs(pixelDepth - surfaceDepth) > DepthTolerance) {
            return false;
        }
    }
    
    float ddxuvx = ddx(vertexUV.x);
    float ddyuvy = ddy(vertexUV.y);
    float2 lowResUV = vertexUV;
    if (!renderFlagUpscale2D(rp.flags)) {
        float2 screenPos = floor(vertexPosition.xy);
        screenPos.x += FbParams.horizontalMisalignment;
        lowResUV -= fmod(screenPos.xy, FbParams.resolutionScale.yy) * float2(ddxuvx, ddyuvy);
    }
    
    int tileIndex0 = 0;
    int tileIndex1 = 1;
    float lodFraction;
    float lodScale = 1.0f;
    if (!renderFlagUpscaleLOD(rp.flags)) {
        lodScale = FbParams.resolutionScale.y;
    }
    
    computeLOD(otherMode, instanceRenderIndices[gConstants.renderIndex].rdpTileCount, instanceRDPParams[instanceIndex].primLOD, lodScale, ddxuvx, ddyuvy, tileIndex0, tileIndex1, lodFraction);

    float4 texVal0 = float4(0.0f, 0.0f, 0.0f, 1.0f);
    float4 texVal1 = float4(0.0f, 0.0f, 0.0f, 1.0f);
    if (renderFlagUsesTexture0(rp.flags)) {
        const uint globalTileIndex = instanceRenderIndices[gConstants.renderIndex].rdpTileIndex + tileIndex0;
        RDPTile rdpTile = RDPTiles[globalTileIndex];
        if (!renderFlagDynamicTiles(rp.flags)) {
            rdpTile.cms = renderCMS0(rp.flags);
            rdpTile.cmt = renderCMT0(rp.flags);
            rdpTile.nativeSampler = renderFlagNativeSampler0(rp.flags);
        }
        
        const GPUTile gpuTile = GPUTiles[globalTileIndex];
        const float2 textureUV = gpuTileFlagHighRes(gpuTile.flags) ? vertexUV : lowResUV;
        texVal0 = sampleTexture(otherMode, rp.flags, textureUV, ddx(vertexUV), ddy(vertexUV), rdpTile, gpuTile, false);
    }
    
    if (renderFlagUsesTexture1(rp.flags)) {
        const bool oneCycleHardwareBug = (otherMode.cycleType() == G_CYC_1CYCLE);
        const uint globalTileIndex = instanceRenderIndices[gConstants.renderIndex].rdpTileIndex + (oneCycleHardwareBug ? tileIndex0 : tileIndex1);
        RDPTile rdpTile = RDPTiles[globalTileIndex];
        if (!renderFlagDynamicTiles(rp.flags)) {
            rdpTile.cms = oneCycleHardwareBug ? renderCMS0(rp.flags) : renderCMS1(rp.flags);
            rdpTile.cmt = oneCycleHardwareBug ? renderCMT0(rp.flags) : renderCMT1(rp.flags);
            rdpTile.nativeSampler = oneCycleHardwareBug ? renderFlagNativeSampler0(rp.flags) : renderFlagNativeSampler1(rp.flags);
        }
        
        const GPUTile gpuTile = GPUTiles[globalTileIndex];
        const float2 textureUV = gpuTileFlagHighRes(gpuTile.flags) ? vertexUV : lowResUV;
        const uint nativeSampler = renderFlagNativeSampler1(rp.flags);
        texVal1 = sampleTexture(otherMode, rp.flags, textureUV, ddx(vertexUV), ddy(vertexUV), rdpTile, gpuTile, oneCycleHardwareBug);
    }
    
    // Color combiner.
    float4 shadeColor = vertexColor;
    float4 combinerColor;
    float alphaCompareValue;
    ColorCombiner::Inputs ccInputs;
    ccInputs.otherMode = otherMode;
    ccInputs.alphaOnly = false;
    ccInputs.texVal0 = texVal0;
    ccInputs.texVal1 = texVal1;
    ccInputs.primColor = instanceRDPParams[instanceIndex].primColor;
    ccInputs.shadeColor = shadeColor;
    ccInputs.envColor = instanceRDPParams[instanceIndex].envColor;
    ccInputs.keyCenter = instanceRDPParams[instanceIndex].keyCenter;
    ccInputs.keyScale = instanceRDPParams[instanceIndex].keyScale;
    ccInputs.lodFraction = lodFraction;
    ccInputs.primLodFrac = instanceRDPParams[instanceIndex].primLOD.x;
    ccInputs.noise = nextRand(randomSeed);
    ccInputs.K4 = (instanceRDPParams[instanceIndex].convertK[4] / 255.0f);
    ccInputs.K5 = (instanceRDPParams[instanceIndex].convertK[5] / 255.0f);
    colorCombiner.run(ccInputs, combinerColor, alphaCompareValue);
    
#if 0
    // Alpha dither.
    // TODO: To avoid increasing the alpha values here, the only viable choice would be to drop the precision down to 5-bit.
    // Since we'd rather keep the full precision of the alpha channel from the texture, this step is ignored for now.
    if (otherMode.alphaDither() != G_AD_DISABLE) {
        uint rgbDither = (otherMode.rgbDither() >> G_MDSFT_RGBDITHER) & 0x3;
        uint alphaDither = (otherMode.alphaDither() >> G_MDSFT_ALPHADITHER) & 0x3;
        float alphaDitherFloat = (AlphaDitherValue(rgbDither, alphaDither, floor(vertexPosition.xy), randomSeed) / 255.0f);
        if (!otherMode.alphaCvgSel()) {
            combinerColor.a += alphaDitherFloat;
        }
        
        shadeColor.a += alphaDitherFloat;
        alphaCompareValue += alphaDitherFloat;
    }
#endif
    
    // Alpha compare.
    if (otherMode.alphaCompare() == G_AC_DITHER) {
        if (alphaCompareValue < nextRand(randomSeed)) {
            return false;
        }
    }
    else if (otherMode.alphaCompare() == G_AC_THRESHOLD) {
        if (alphaCompareValue < instanceRDPParams[instanceIndex].blendColor.a) {
            return false;
        }
    }
    
    // Compute coverage estimation.
    const bool usesHDR = renderFlagUsesHDR(rp.flags);
    const float cvgRange = usesHDR ? 65535.0f : 255.0f;
    float resultCvg = (8.0f / cvgRange) * (otherMode.cvgXAlpha() ? combinerColor.a : 1.0f);
    
    // Discard all pixels without coverage.
    const float CoverageThreshold = 1.0f / cvgRange;
    if (resultCvg < CoverageThreshold) {
        return false;
    }
    
    // Add the blender if it can be replicated with simple emulation.
    Blender::Inputs blInputs;
    blInputs.blendColor = instanceRDPParams[instanceIndex].blendColor;
    blInputs.fogColor = instanceRDPParams[instanceIndex].fogColor;
    blInputs.shadeAlpha = shadeColor.a;
    resultColor = Blender::run(otherMode, rp.flags, blInputs, combinerColor, false);
    resultAlpha = 1.0f;
    
    // When using alpha blending, we store the blending factor into the dedicated output so the main one can be used for coverage.
    const bool alphaBlend = (otherMode.cycleType() != G_CYC_COPY) && Blender::usesAlphaBlend(otherMode);
    if (alphaBlend) {
        resultAlpha.a = resultColor.a;
    }
    
    // Preserve the value in the destination.
    if (otherMode.cvgDst() == CVG_DST_SAVE) {
        resultColor.a = 0.0f;
    }
    // Write a full coverage value regardless of the computed coverage.
    else if (otherMode.cvgDst() == CVG_DST_FULL) {
        resultColor.a = 7.0f / cvgRange;
    }
    // Write the coverage value clamped to the full value allowed.
    else if (otherMode.cvgDst() == CVG_DST_CLAMP) {
        resultColor.a = min(resultCvg, 7.0f / cvgRange);
    }
    // Write out the computed coverage. It'll be added on wrap mode.
    else {
        resultColor.a = resultCvg;
    }
    
    // Add highlight color to the last step.
    uint highlightColorUint = instanceRenderIndices[gConstants.renderIndex].highlightColor;
    if (highlightColorUint > 0) {
        float4 highlightColor = RGBA32ToFloat4(highlightColorUint);
        resultColor = lerp(resultColor, highlightColor, highlightColor.a);
    }
    
#ifdef DYNAMIC_RENDER_PARAMS
    if (FrParams.viewUbershaders) {
        resultColor.rgb = lerp(resultColor.rgb, float3(1.0f, 0.0f, 0.0f), 0.5f);
    }
#endif
    
    return true;
}

#if defined(DYNAMIC_RENDER_PARAMS)
RenderParams getRenderParams() {
    uint instanceIndex = instanceRenderIndices[gConstants.renderIndex].instanceIndex;
    return DynamicRenderParams[instanceIndex];
}
#elif defined(SPEC_CONSTANT_RENDER_PARAMS)
#   include "RenderParamsSpecConstants.hlsli"
#endif

#if defined(DYNAMIC_RENDER_PARAMS) || defined(SPEC_CONSTANT_RENDER_PARAMS)
void PSMain(
      in float4 vertexPosition : SV_POSITION
    , in float2 vertexUV : TEXCOORD
    , in float4 vertexSmoothColor : COLOR0
#if defined(DYNAMIC_RENDER_PARAMS) || defined(VERTEX_FLAT_COLOR)
    , nointerpolation in float4 vertexFlatColor : COLOR1
#endif
#if defined(DYNAMIC_RENDER_PARAMS)
    , bool isFrontFace : SV_IsFrontFace
#endif
    , [[vk::location(0)]] [[vk::index(0)]] out float4 pixelColor : SV_TARGET0
    , [[vk::location(0)]] [[vk::index(1)]] out float4 pixelAlpha : SV_TARGET1
)
{
#if !defined(DYNAMIC_RENDER_PARAMS)
#if !defined(VERTEX_FLAT_COLOR)
    float4 vertexFlatColor = 0.0f;
#endif
    bool isFrontFace = false;
#endif
    float4 resultColor;
    float4 resultAlpha;
    float resultDepth;
    if (!RasterPS(getRenderParams(), vertexPosition, vertexUV, vertexSmoothColor, vertexFlatColor, isFrontFace, resultColor, resultAlpha)) {
        discard;
    }

    pixelColor = resultColor;
    pixelAlpha = resultAlpha;
}
#endif