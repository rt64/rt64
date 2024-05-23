//
// RT64
//

#pragma once

#include "Math.hlsli"
#include "TextureDecoder.hlsli"

#include "shared/rt64_other_mode.h"
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

float4 clampWrapMirrorSample(const RDPTile rdpTile, const GPUTile gpuTile, int2 texelInt, uint textureIndex, uint tlut, bool canDecodeTMEM, bool usesHDR) {
    if (rdpTile.cms & G_TX_CLAMP) {
        texelInt.x = clamp(texelInt.x, 0, (round(gpuTile.tcScale.x * rdpTile.lrs) / 4) - (round(gpuTile.tcScale.x * rdpTile.uls) / 4) + round(gpuTile.tcScale.x - 1.0f));
    }

    if (rdpTile.cmt & G_TX_CLAMP) {
        texelInt.y = clamp(texelInt.y, 0, (round(gpuTile.tcScale.y * rdpTile.lrt) / 4) - (round(gpuTile.tcScale.y * rdpTile.ult) / 4) + round(gpuTile.tcScale.y - 1.0f));
    }
    
    const int masks = round(rdpTile.masks * gpuTile.tcScale.x);
    if ((rdpTile.cms & G_TX_MIRROR) && (masks > 0) && (modulo(texelInt.x, masks * 2) >= masks)) {
        texelInt.x = (masks - 1) - modulo(texelInt.x, masks);
    }
    else {
        texelInt.x = modulo(texelInt.x, masks);
    }

    const int maskt = round(rdpTile.maskt * gpuTile.tcScale.y);
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
        float4 textureColor = gTextures[NonUniformResourceIndex(textureIndex)].Load(int3(texelInt, 0));
        
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

float4 sampleTexture(OtherMode otherMode, RenderFlags renderFlags, float2 inputUV, uint textureIndex, const RDPTile rdpTile, const GPUTile gpuTile, bool nextPixelBug) {
    const float LowPrecision = 128.0f;
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
    int2 texelBaseInt = floor(uvCoord);
    float4 sample00 = clampWrapMirrorSample(rdpTile, gpuTile, texelBaseInt + int2(0, 0), textureIndex, tlut, canDecodeTMEM, usesHDR);
    if (filterBilerp || linearFiltering) {
        float2 fracPart = uvCoord - texelBaseInt;
        float4 sample01 = clampWrapMirrorSample(rdpTile, gpuTile, texelBaseInt + int2(0, 1), textureIndex, tlut, canDecodeTMEM, usesHDR);
        float4 sample10 = clampWrapMirrorSample(rdpTile, gpuTile, texelBaseInt + int2(1, 0), textureIndex, tlut, canDecodeTMEM, usesHDR);
        float4 sample11 = clampWrapMirrorSample(rdpTile, gpuTile, texelBaseInt + int2(1, 1), textureIndex, tlut, canDecodeTMEM, usesHDR);
        if (linearFiltering) {
            return lerp(lerp(sample00, sample10, fracPart.x), lerp(sample01, sample11, fracPart.x), fracPart.y);
        }
        else {
            const bool midTexelAverage = (filter == G_TF_AVERAGE);
            const bool useAverage = midTexelAverage && all(abs(fracPart - 0.5f) <= (1.0f / LowPrecision));
            if (midTexelAverage && useAverage) {
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