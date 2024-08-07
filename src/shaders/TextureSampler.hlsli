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

float4 clampWrapMirrorSample(const RDPTile rdpTile, const GPUTile gpuTile, float2 tcScale, int2 texelInt, uint textureIndex, uint tlut, bool canDecodeTMEM, uint mipLevel, bool usesHDR) {
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
    if (canDecodeTMEM && gpuTileFlagRawTMEM(gpuTile.flags)) {
        return sampleTMEM(texelInt, rdpTile.siz, rdpTile.fmt, rdpTile.address, rdpTile.stride, tlut, rdpTile.palette, gTMEM[NonUniformResourceIndex(textureIndex)]);
    }
    // Sample the color version directly.
    else {
        float4 textureColor = gTextures[NonUniformResourceIndex(textureIndex)].Load(int3(texelInt, mipLevel));
        
        // Alpha channel in framebuffer textures represent the coverage. A modulo operation must be performed
        // to get the value that would correspond to the alpha channel when it's sampled.
        if (gpuTileFlagAlphaIsCvg(gpuTile.flags)) {
            const float cvgRange = usesHDR ? 65535.0f : 255.0f;
            int cvgModulo = round(textureColor.a * cvgRange) % 8;
            textureColor.a = (cvgModulo & 0x4) ? 1.0f : 0.0f;
        }
        
        return textureColor;
    }
}

static const float LowPrecision = 128.0f;

float4 sampleTextureLevel(const RDPTile rdpTile, const GPUTile gpuTile, bool filterBilerp, bool filterAverage, bool linearFiltering, float2 uvCoord, uint textureIndex, uint tlut, bool canDecodeTMEM, uint mipLevel, bool usesHDR) {
    float2 tcScale = gpuTile.tcScale;
    float mipScale = float(1U << mipLevel);
    uvCoord /= mipScale;
    tcScale /= mipScale;
    
    int2 texelBaseInt = floor(uvCoord);
    float4 sample00 = clampWrapMirrorSample(rdpTile, gpuTile, tcScale, texelBaseInt + int2(0, 0), textureIndex, tlut, canDecodeTMEM, mipLevel, usesHDR);
    if (filterBilerp || linearFiltering) {
        float2 fracPart = uvCoord - texelBaseInt;
        float4 sample01 = clampWrapMirrorSample(rdpTile, gpuTile, tcScale, texelBaseInt + int2(0, 1), textureIndex, tlut, canDecodeTMEM, mipLevel, usesHDR);
        float4 sample10 = clampWrapMirrorSample(rdpTile, gpuTile, tcScale, texelBaseInt + int2(1, 0), textureIndex, tlut, canDecodeTMEM, mipLevel, usesHDR);
        float4 sample11 = clampWrapMirrorSample(rdpTile, gpuTile, tcScale, texelBaseInt + int2(1, 1), textureIndex, tlut, canDecodeTMEM, mipLevel, usesHDR);
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

float4 sampleTexture(OtherMode otherMode, RenderFlags renderFlags, float2 inputUV, float2 ddxUVx, float2 ddyUVy, uint textureIndex, const RDPTile rdpTile, const GPUTile gpuTile, bool nextPixelBug) {
    const bool texturePerspective = (otherMode.textPersp() == G_TP_PERSP);
    const bool applyCorrection = (!texturePerspective && !renderFlagRect(renderFlags));

    // Compensates the lack of perspective division for the UVs based on observed hardware behavior.
    const float perspCorrectionMod = applyCorrection ? 0.5f : 1.0f;
    float2 uvCoord = inputUV * perspCorrectionMod * float2(rdpTile.shifts, rdpTile.shiftt);

#if SIMULATE_LOW_PRECISION
    // Simulates the lower precision of the hardware's coordinate interpolation.
    uvCoord = round(uvCoord * LowPrecision) / LowPrecision;
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
    if (!gpuTileFlagHasMipmaps(gpuTile.flags)) {
        return sampleTextureLevel(rdpTile, gpuTile, filterBilerp, filterAverage, linearFiltering, uvCoord, textureIndex, tlut, canDecodeTMEM, 0, usesHDR);
    }
    else {
        // Retrieve the dimensions of the texture for either type of sampler.
        Texture2D texture = gTextures[NonUniformResourceIndex(textureIndex)];
        uint textureWidth, textureHeight, textureLevels;
        texture.GetDimensions(0, textureWidth, textureHeight, textureLevels);
        
        if (nativeSampler == NATIVE_SAMPLER_NONE) {
            float2 ddxUVxScaled = ddxUVx * gpuTile.tcScale;
            float2 ddxUVyScaled = ddyUVy * gpuTile.tcScale;
            float ddMax = max(dot(ddxUVxScaled, ddxUVxScaled), dot(ddxUVyScaled, ddxUVyScaled));
            float mipBias = -0.5f;
            float mip = 0.5 * log2(ddMax) + mipBias;
            float maxMip = float(textureLevels - 1);
            float4 hiLevel = sampleTextureLevel(rdpTile, gpuTile, filterBilerp, filterAverage, linearFiltering, uvCoord, textureIndex, tlut, canDecodeTMEM, min(floor(mip), maxMip), usesHDR);
            float4 loLevel = sampleTextureLevel(rdpTile, gpuTile, filterBilerp, filterAverage, linearFiltering, uvCoord, textureIndex, tlut, canDecodeTMEM, min(floor(mip) + 1, maxMip), usesHDR);
            return lerp(hiLevel, loLevel, frac(mip));
        }
        else {
            // Must normalize the texture coordinate and the derivatives.
            float2 nativeUVCoord = uvCoord / float2(textureWidth, textureHeight);
            float2 originalSize = float2(textureWidth, textureHeight) / gpuTile.tcScale;
            float2 ddxUVxNorm = ddxUVx / originalSize;
            float2 ddxUVyNorm = ddyUVy / originalSize;
            
            // Choose the native sampler that was determined to be compatible.
            switch (nativeSampler) {
                case NATIVE_SAMPLER_WRAP_WRAP:
                    return texture.SampleGrad(gWrapWrapSampler, nativeUVCoord, ddxUVxNorm, ddxUVyNorm);
                case NATIVE_SAMPLER_WRAP_MIRROR:
                    return texture.SampleGrad(gWrapMirrorSampler, nativeUVCoord, ddxUVxNorm, ddxUVyNorm);
                case NATIVE_SAMPLER_WRAP_CLAMP:
                    return texture.SampleGrad(gWrapClampSampler, nativeUVCoord, ddxUVxNorm, ddxUVyNorm);
                case NATIVE_SAMPLER_MIRROR_WRAP:
                    return texture.SampleGrad(gMirrorWrapSampler, nativeUVCoord, ddxUVxNorm, ddxUVyNorm);
                case NATIVE_SAMPLER_MIRROR_MIRROR:
                    return texture.SampleGrad(gMirrorMirrorSampler, nativeUVCoord, ddxUVxNorm, ddxUVyNorm);
                case NATIVE_SAMPLER_MIRROR_CLAMP:
                    return texture.SampleGrad(gMirrorClampSampler, nativeUVCoord, ddxUVxNorm, ddxUVyNorm);
                case NATIVE_SAMPLER_CLAMP_WRAP:
                    return texture.SampleGrad(gClampWrapSampler, nativeUVCoord, ddxUVxNorm, ddxUVyNorm);
                case NATIVE_SAMPLER_CLAMP_MIRROR:
                    return texture.SampleGrad(gClampMirrorSampler, nativeUVCoord, ddxUVxNorm, ddxUVyNorm);
                case NATIVE_SAMPLER_CLAMP_CLAMP:
                default:
                    return texture.SampleGrad(gClampClampSampler, nativeUVCoord, ddxUVxNorm, ddxUVyNorm);
            }
        }
    }
}