//
// RT64
//

#pragma once

#include "FbRendererCommon.hlsli"
#include "Math.hlsli"
#include "TextureDecoder.hlsli"

#include "shared/rt64_other_mode.h"
#include "shared/rt64_render_flags.h"
#include "shared/rt64_render_params.h"

#define SIMULATE_LOW_PRECISION 1
#define FIX_UPSCALING_PRECISION 1

// We prefer using for loops to reduce the size of the final ubershader as much as possible.
// When using specialized shaders, we prefer to unroll the loop manually to give re-spirv an
// easier time to eliminate dead code, as its graph model can't handle the loop constructs yet.
#ifdef DYNAMIC_RENDER_PARAMS
#define USE_FOR_LOOPS 1
#else
#define USE_FOR_LOOPS 0
#endif

void computeLOD(OtherMode otherMode, uint rdpTileCount, float2 primLOD, float resLodScale, float ddxuvx, float ddyuvy, inout int tileIndex0, inout int tileIndex1, out float lodFraction) {
    const bool usesLOD = (otherMode.textLOD() == G_TL_LOD);
    if (usesLOD) {
        const bool lodSharpen = (otherMode.textDetail() & G_TD_SHARPEN) != 0;
        const bool lodDetail = (otherMode.textDetail() & G_TD_DETAIL) != 0;
        const int tileMax = int(rdpTileCount) - 1;
        float maxDst = max(abs(ddxuvx), abs(ddyuvy)) * resLodScale;
        if (lodDetail || lodSharpen) {
            maxDst = max(maxDst, primLOD.y);
        }

        int tileBase = floor(log2(maxDst));
        lodFraction = maxDst / pow(2, max(tileBase, 0)) - 1.0f;

        if (lodSharpen) {
            if (maxDst < 1.0f)
                lodFraction = maxDst - 1.0f;
        }

        if (lodDetail) {
            if (lodFraction < 0.0f)
                lodFraction = maxDst;
            tileBase += 1;
        }
        else {
            if (tileBase >= tileMax) {
                lodFraction = 1.0f;
            }
        }

        if (lodDetail || lodSharpen) {
            tileBase = max(tileBase, 0);
        }
        else {
            lodFraction = max(lodFraction, 0.0f);
        }

        tileIndex0 = clamp(tileBase, 0, tileMax);
        tileIndex1 = clamp(tileBase + 1, 0, tileMax);
    }
    else {
        lodFraction = 1.0f;

    }
}

float4 clampWrapMirrorSample(const RDPTile rdpTile, const GPUTile gpuTile, float2 tcScale, int2 texelInt, uint tlut, bool gpuTileUsesTMEM, uint mipLevel) {
    if (rdpTile.cms & G_TX_CLAMP) {
        texelInt.x = clamp(texelInt.x, 0, (round(tcScale.x * rdpTile.lrs) / 4) - (round(tcScale.x * rdpTile.uls) / 4) + round(tcScale.x - 1.0f));
    }

    if (rdpTile.cmt & G_TX_CLAMP) {
        texelInt.y = clamp(texelInt.y, 0, (round(tcScale.y * rdpTile.lrt) / 4) - (round(tcScale.y * rdpTile.ult) / 4) + round(tcScale.y - 1.0f));
    }
    
    const int masks = round(rdpTile.masks * tcScale.x);
    if ((rdpTile.cms & G_TX_MIRROR) && (masks > 0) && (modulo(texelInt.x, masks * 2) >= masks)) {
        texelInt.x = (masks - 1) - modulo(texelInt.x, masks);
    }
    else {
        texelInt.x = modulo(texelInt.x, masks);
    }

    const int maskt = round(rdpTile.maskt * tcScale.y);
    if ((rdpTile.cmt & G_TX_MIRROR) && (maskt > 0) && (modulo(texelInt.y, maskt * 2) >= maskt)) {
        texelInt.y = (maskt - 1) - modulo(texelInt.y, maskt);
    }
    else {
        texelInt.y = modulo(texelInt.y, maskt);
    }
    
    // Allows selection of only particular columns or rows as defined by the tile.
    texelInt = (texelInt & gpuTile.texelMask) + gpuTile.texelShift;

    // Check if tile requires TMEM decoding and sample using dynamic decoding.
    if (gpuTileUsesTMEM) {
        return sampleTMEM(texelInt, rdpTile.siz, rdpTile.fmt, rdpTile.address, rdpTile.stride, tlut, rdpTile.palette, gTMEM[NonUniformResourceIndex(gpuTile.textureIndex)]);
    }
    // Sample the color version directly.
    else {
        return gTextures[NonUniformResourceIndex(gpuTile.textureIndex)].Load(int3(texelInt, mipLevel));
    }
}

static const float LowPrecision = 128.0f;

float4 sampleTextureNative(Texture2D texture, uint nativeSampler, int2 texelBaseInt, float2 textureSize) {
    // Transform to native coordinate. Half pixel offset is required to reach the desired texel.
    float2 nativeUVCoord = (float2(texelBaseInt) + 0.5f) / textureSize;
            
    // Choose the native sampler that was determined to be compatible.
    switch (nativeSampler) {
        case NATIVE_SAMPLER_WRAP_WRAP:
            return texture.SampleLevel(gNearestWrapWrapSampler, nativeUVCoord, 0.0f);
        case NATIVE_SAMPLER_WRAP_MIRROR:
            return texture.SampleLevel(gNearestWrapMirrorSampler, nativeUVCoord, 0.0f);
        case NATIVE_SAMPLER_WRAP_CLAMP:
            return texture.SampleLevel(gNearestWrapClampSampler, nativeUVCoord, 0.0f);
        case NATIVE_SAMPLER_MIRROR_WRAP:
            return texture.SampleLevel(gNearestMirrorWrapSampler, nativeUVCoord, 0.0f);
        case NATIVE_SAMPLER_MIRROR_MIRROR:
            return texture.SampleLevel(gNearestMirrorMirrorSampler, nativeUVCoord, 0.0f);
        case NATIVE_SAMPLER_MIRROR_CLAMP:
            return texture.SampleLevel(gNearestMirrorClampSampler, nativeUVCoord, 0.0f);
        case NATIVE_SAMPLER_CLAMP_WRAP:
            return texture.SampleLevel(gNearestClampWrapSampler, nativeUVCoord, 0.0f);
        case NATIVE_SAMPLER_CLAMP_MIRROR:
            return texture.SampleLevel(gNearestClampMirrorSampler, nativeUVCoord, 0.0f);
        case NATIVE_SAMPLER_CLAMP_CLAMP:
        default:
            return texture.SampleLevel(gNearestClampClampSampler, nativeUVCoord, 0.0f);
    }
}

float4 sampleTextureLevel(const RDPTile rdpTile, const GPUTile gpuTile, bool filterBilerp, bool filterAverage, bool linearFiltering, float2 uvCoord, uint tlut, bool canDecodeTMEM, uint mipLevel, bool usesHDR) {
    float2 tcScale = gpuTile.tcScale;
    float mipScale = float(1U << mipLevel);
    uvCoord /= mipScale;
    tcScale /= mipScale;
    
    int2 texelBaseInt = floor(uvCoord);
    bool filtering = or(filterBilerp, linearFiltering);
    float4 samples[4];
    const uint nativeSampler = rdpTile.nativeSampler;
    bool gpuTileUsesTMEM = canDecodeTMEM && gpuTileFlagRawTMEM(gpuTile.flags);
    if ((nativeSampler == NATIVE_SAMPLER_NONE) || gpuTileUsesTMEM) {
#if USE_FOR_LOOPS
        int numSamples = select_uint(filtering, 4, 1);
        [unroll]
        for (int i = 0; i < numSamples; i++) {
            samples[i] = clampWrapMirrorSample(rdpTile, gpuTile, tcScale, texelBaseInt + int2(i >> 1, i & 1), tlut, gpuTileUsesTMEM, mipLevel);
        }
#else
        samples[0] = clampWrapMirrorSample(rdpTile, gpuTile, tcScale, texelBaseInt, tlut, gpuTileUsesTMEM, mipLevel);
        
        if (filtering) {
            samples[1] = clampWrapMirrorSample(rdpTile, gpuTile, tcScale, texelBaseInt + int2(0, 1), tlut, gpuTileUsesTMEM, mipLevel);
            samples[2] = clampWrapMirrorSample(rdpTile, gpuTile, tcScale, texelBaseInt + int2(1, 0), tlut, gpuTileUsesTMEM, mipLevel);
            samples[3] = clampWrapMirrorSample(rdpTile, gpuTile, tcScale, texelBaseInt + int2(1, 1), tlut, gpuTileUsesTMEM, mipLevel);
        }
#endif
    }
    else {
        Texture2D texture = gTextures[NonUniformResourceIndex(gpuTile.textureIndex)];
#if USE_FOR_LOOPS
        int numSamples = select_uint(filtering, 4, 1);
        [unroll]
        for (int i = 0; i < numSamples; i++) {
            samples[i] = sampleTextureNative(texture, nativeSampler, texelBaseInt + int2(i >> 1, i & 1), gpuTile.textureDimensions.xy);
        }
#else
        samples[0] = sampleTextureNative(texture, nativeSampler, texelBaseInt, gpuTile.textureDimensions.xy);
        
        if (filtering) {
            samples[1] = sampleTextureNative(texture, nativeSampler, texelBaseInt + int2(0, 1), gpuTile.textureDimensions.xy);
            samples[2] = sampleTextureNative(texture, nativeSampler, texelBaseInt + int2(1, 0), gpuTile.textureDimensions.xy);
            samples[3] = sampleTextureNative(texture, nativeSampler, texelBaseInt + int2(1, 1), gpuTile.textureDimensions.xy);
        }
#endif
    }
    
    float4 sample00 = samples[0];
    if (filtering) {
        float2 fracPart = uvCoord - texelBaseInt;
        float4 sample01 = samples[1];
        float4 sample10 = samples[2];
        float4 sample11 = samples[3];
        if (linearFiltering) {
            return lerp(lerp(sample00, sample10, fracPart.x), lerp(sample01, sample11, fracPart.x), fracPart.y);
        }
        else {
            const bool useAverage = filterAverage && all(abs(fracPart - 0.5f) <= (1.0f / LowPrecision));
            if (filterAverage && useAverage) {
                return (sample00 + sample01 + sample10 + sample11) / 4.0f;
            }
            else {
            // Originally written by ArthurCarvalho
            // Sourced from https://www.emutalk.net/threads/emulating-nintendo-64-3-sample-bilinear-filtering-using-shaders.54215/
                float4 tri0 = lerp(sample00, sample10, fracPart.x) + (sample01 - sample00) * fracPart.y;
                float4 tri1 = lerp(sample11, sample01, 1.0f - fracPart.x) + (sample10 - sample11) * (1.0f - fracPart.y);
                return lerp(tri0, tri1, step(1.0f, fracPart.x + fracPart.y));
            }
        }
    }
    else {
        return sample00;
    }
}

float4 sampleTexture(OtherMode otherMode, RenderFlags renderFlags, float2 inputUV, float2 ddxUVx, float2 ddyUVy, const RDPTile rdpTile, const GPUTile gpuTile, bool nextPixelBug) {
    const bool texturePerspective = (otherMode.textPersp() == G_TP_PERSP);
    const bool applyCorrection = (!texturePerspective && !renderFlagRect(renderFlags));

    // Compensates the lack of perspective division for the UVs based on observed hardware behavior.
    const float perspCorrectionMod = applyCorrection ? 0.5f : 1.0f;
    float2 uvCoord = inputUV * perspCorrectionMod * float2(rdpTile.shifts, rdpTile.shiftt);

#if SIMULATE_LOW_PRECISION
    if (!gpuTileFlagHighRes(gpuTile.flags)) {
        // Simulates the lower precision of the hardware's coordinate interpolation.
        // This is not simulated if a texture replacement is being used.
        uvCoord = round(uvCoord * LowPrecision) / LowPrecision;
    }
#endif
    
    // Simulate the next pixel bug when it's a rect by just shifting the UV coordinates to the right by one pixel.
    // The actual bug probably requires wrapping around in texture memory to the next row when on the edge instead.
    if (nextPixelBug && renderFlagRect(renderFlags)) {
        uvCoord.x += 1.0f;
    }
    
    uvCoord *= gpuTile.tcScale;
    uvCoord -= (float2(rdpTile.uls, rdpTile.ult) * gpuTile.ulScale) / 4.0f;
    
    const uint filter = otherMode.textFilt();
    const uint cycleType = otherMode.cycleType();
    const bool filterBilerp = (filter != G_TF_POINT) && (cycleType != G_CYC_COPY);
    const bool filterAverage = (filter == G_TF_AVERAGE);
    const bool linearFiltering = renderFlagLinearFiltering(renderFlags);
    
#if FIX_UPSCALING_PRECISION
    // Account for the fact that scaling can result in less than perfect results that affect the quality of point filtering.
    // This helps with artifacts caused by copying high resolution tiles onto other textures in non-square ratios.
    if (!filterBilerp && !linearFiltering && gpuTileFlagHighRes(gpuTile.flags)) {
        const float HighResPrecision = 8.0f;
        uvCoord = round(uvCoord * HighResPrecision) / HighResPrecision;
    }
#endif
    
    const uint tlut = otherMode.textLUT();
    const bool canDecodeTMEM = renderFlagCanDecodeTMEM(renderFlags);
    const bool usesHDR = renderFlagUsesHDR(renderFlags);
    const uint nativeSampler = rdpTile.nativeSampler;
    const bool flagHasMipmaps = gpuTileFlagHasMipmaps(gpuTile.flags);
    uint numRDPSamples = 0;
    uint RDPMipLevels[2];
    RDPMipLevels[0] = 0;
    RDPMipLevels[1] = 0;
    float mip;

    // Determine the RDP sample count and mip levels.
    if (flagHasMipmaps) {
        // Retrieve the dimensions of the texture for either type of sampler.
        Texture2D texture = gTextures[NonUniformResourceIndex(gpuTile.textureIndex)];
        if (nativeSampler == NATIVE_SAMPLER_NONE) {
            float2 ddxUVxScaled = ddxUVx * gpuTile.tcScale;
            float2 ddxUVyScaled = ddyUVy * gpuTile.tcScale;
            float ddMax = max(dot(ddxUVxScaled, ddxUVxScaled), dot(ddxUVyScaled, ddxUVyScaled));
            float mipBias = -0.25f;
            mip = 0.5 * log2(ddMax) + mipBias;
            float maxMip = float(gpuTile.textureDimensions.z - 1);
            RDPMipLevels[0] = min(floor(mip), maxMip);
            RDPMipLevels[1] = min(floor(mip) + 1, maxMip);
            numRDPSamples = 2;
        }
        else {
            // Must normalize the texture coordinate and the derivatives.
            float2 nativeUVCoord = uvCoord / gpuTile.textureDimensions.xy;
            float2 originalSize = gpuTile.textureDimensions.xy / gpuTile.tcScale;
            float2 ddxUVxNorm = ddxUVx / originalSize;
            float2 ddxUVyNorm = ddyUVy / originalSize;
            
            // Choose the native sampler that was determined to be compatible.
            switch (nativeSampler) {
                case NATIVE_SAMPLER_WRAP_WRAP:
                    return texture.SampleGrad(gLinearWrapWrapSampler, nativeUVCoord, ddxUVxNorm, ddxUVyNorm);
                case NATIVE_SAMPLER_WRAP_MIRROR:
                    return texture.SampleGrad(gLinearWrapMirrorSampler, nativeUVCoord, ddxUVxNorm, ddxUVyNorm);
                case NATIVE_SAMPLER_WRAP_CLAMP:
                    return texture.SampleGrad(gLinearWrapClampSampler, nativeUVCoord, ddxUVxNorm, ddxUVyNorm);
                case NATIVE_SAMPLER_MIRROR_WRAP:
                    return texture.SampleGrad(gLinearMirrorWrapSampler, nativeUVCoord, ddxUVxNorm, ddxUVyNorm);
                case NATIVE_SAMPLER_MIRROR_MIRROR:
                    return texture.SampleGrad(gLinearMirrorMirrorSampler, nativeUVCoord, ddxUVxNorm, ddxUVyNorm);
                case NATIVE_SAMPLER_MIRROR_CLAMP:
                    return texture.SampleGrad(gLinearMirrorClampSampler, nativeUVCoord, ddxUVxNorm, ddxUVyNorm);
                case NATIVE_SAMPLER_CLAMP_WRAP:
                    return texture.SampleGrad(gLinearClampWrapSampler, nativeUVCoord, ddxUVxNorm, ddxUVyNorm);
                case NATIVE_SAMPLER_CLAMP_MIRROR:
                    return texture.SampleGrad(gLinearClampMirrorSampler, nativeUVCoord, ddxUVxNorm, ddxUVyNorm);
                case NATIVE_SAMPLER_CLAMP_CLAMP:
                default:
                    return texture.SampleGrad(gLinearClampClampSampler, nativeUVCoord, ddxUVxNorm, ddxUVyNorm);
            }
        }
    }
    else {
        numRDPSamples = 1;
    }
    
    float4 textureSamples[2];

    // Perform the RDP sampling.
#if USE_FOR_LOOPS
    [unroll]
    for (uint i = 0; i < numRDPSamples; i++) {
        textureSamples[i] = sampleTextureLevel(rdpTile, gpuTile, filterBilerp, filterAverage, linearFiltering, uvCoord, tlut, canDecodeTMEM, RDPMipLevels[i], usesHDR);
    }
#else
    textureSamples[0] = sampleTextureLevel(rdpTile, gpuTile, filterBilerp, filterAverage, linearFiltering, uvCoord, tlut, canDecodeTMEM, RDPMipLevels[0], usesHDR);
    
    if (numRDPSamples > 1) {
        textureSamples[1] = sampleTextureLevel(rdpTile, gpuTile, filterBilerp, filterAverage, linearFiltering, uvCoord, tlut, canDecodeTMEM, RDPMipLevels[1], usesHDR);
    }
#endif

    // Compute the result from the RDP samples.
    float4 textureColor;
    if (!flagHasMipmaps) {
        textureColor = textureSamples[0];
    }
    else {
        float4 hiLevel = textureSamples[0];
        float4 loLevel = textureSamples[1];
        textureColor = lerp(hiLevel, loLevel, frac(mip));
    }
    
    // Alpha channel in framebuffer textures represent the coverage. A modulo operation must be performed
    // to get the value that would correspond to the alpha channel when it's sampled.
    if (gpuTileFlagAlphaIsCvg(gpuTile.flags)) {
        const float cvgRange = usesHDR ? 65535.0f : 255.0f;
        int cvgModulo = round(textureColor.a * cvgRange) % 8;
        textureColor.a = (cvgModulo & 0x4) ? 1.0f : 0.0f;
    }
    
    return textureColor;
}