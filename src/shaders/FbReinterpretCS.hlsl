//
// RT64
//

#include "shared/rt64_f3d_defines.h"
#include "shared/rt64_fb_common.h"
#include "shared/rt64_fb_reinterpret.h"

#include "Depth.hlsli"
#include "Random.hlsli"
#include "TextureDecoder.hlsli"

[[vk::push_constant]] ConstantBuffer<FbReinterpretCB> gConstants : register(b0);
Texture2D<float4> gInputColor : register(t1);
Texture1D<uint> gInputTLUT : register(t2);
RWTexture2D<float4> gOutput : register(u3);

float4 RGBA16toCI8(float4 inputColor, uint2 inputCoord, uint2 outputCoord) {
    // Drop down the input color to its RGBA16 version.
    uint2 ditherCoord = inputCoord + gConstants.ditherOffset;
    uint randomSeed = initRand(gConstants.ditherRandomSeed, ditherCoord.y * gConstants.resolution.x + ditherCoord.x, 16);
    uint ditherValue = DitherPatternValue(gConstants.ditherPattern, ditherCoord, randomSeed);
    uint nativeColor = Float4ToRGBA16(inputColor, ditherValue, gConstants.usesHDR);

    // Extract the lower or upper half of the value depending on the pixel misalignment.
    uint pixelMisalignment = 1 - (outputCoord.x % 2);
    uint pixelValue = (nativeColor >> (8 * pixelMisalignment)) & 0xFF;
    uint paletteAddress = RDP_TMEM_PALETTE + (pixelValue << 3);
    Texture1D<uint> TMEM = gInputTLUT;
    uint paletteValue = loadTLUT(paletteAddress + 1) | (loadTLUT(paletteAddress) << 8);
    uint decodedFormat = gConstants.tlutFormat - 1;
    switch (decodedFormat) {
    case G_TT_RGBA16:
        return RGBA16ToFloat4(paletteValue);
    case G_TT_IA16:
        return IA16ToFloat4(paletteValue);
    default:
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

uint ANY8toUINT8(float4 inputColor, uint2 inputCoord) {
    // Grab the R or G channel based on whether the input column is odd or not.
    bool oddColumn = inputCoord.x & 1;
    float inputChannel = oddColumn ? inputColor.g : inputColor.r;

    // Drop down the input to its I8 version.
    return FloatToUINT8(inputChannel);
}

float4 ANY8toI8(float4 inputColor, uint2 inputCoord, uint2 outputCoord) {
    uint nativeColor = ANY8toUINT8(inputColor, inputCoord);
    return I8ToFloat4(nativeColor);
}

float4 ANY8toIA8(float4 inputColor, uint2 inputCoord, uint2 outputCoord) {
    uint nativeColor = ANY8toUINT8(inputColor, inputCoord);
    return IA8ToFloat4(nativeColor);
}

float4 RGBA16toIA16(float4 inputColor, uint2 inputCoord, uint2 outputCoord) {
    // We actually skip encoding and decoding with native format and do a 
    // similar algorithm that preserves the intention of the effect along
    // with the  precision. The input is encoded into a fake "R10G10B10A2" 
    // format and decoded as another fake "I16A16" format.
    const float InputScale = 1023.0f;
    const float OutputScale = 65535.0f;
    uint4 rgba = round(clamp(inputColor * InputScale, 0.0f, InputScale));
    uint nativeInput = (rgba.r << 22) | (rgba.g << 12) | (rgba.b << 2) | ((rgba.a > 0) ? 3 : 0);
    float iFloat = ((nativeInput >> 16) & 0xFFFF) / OutputScale;
    float aFloat = (nativeInput & 0xFFFF) / OutputScale;
    return float4(iFloat, iFloat, iFloat, aFloat);
}

[numthreads(FB_COMMON_WORKGROUP_SIZE, FB_COMMON_WORKGROUP_SIZE, 1)]
void CSMain(uint2 coord : SV_DispatchThreadID) {
    if ((coord.x < gConstants.resolution.x) && (coord.y < gConstants.resolution.y)) {
        uint2 inputCoord = uint2(floor(coord.x * gConstants.sampleScale), coord.y);
        float4 inputColor = gInputColor.Load(uint3(inputCoord, 0));
        float4 outputColor;
        if ((gConstants.srcFmt == G_IM_FMT_RGBA) && (gConstants.srcSiz == G_IM_SIZ_16b) && (gConstants.dstSiz == G_IM_SIZ_8b) && (gConstants.tlutFormat > 0)) {
            outputColor = RGBA16toCI8(inputColor, inputCoord, coord);
        }
        else if ((gConstants.srcSiz == G_IM_SIZ_8b) && ((gConstants.dstFmt == G_IM_FMT_CI) || (gConstants.dstFmt == G_IM_FMT_I)) && (gConstants.dstSiz == G_IM_SIZ_8b)) {
            outputColor = ANY8toI8(inputColor, inputCoord, coord);
        }
        else if ((gConstants.srcSiz == G_IM_SIZ_8b) && (gConstants.dstFmt == G_IM_FMT_IA) && (gConstants.dstSiz == G_IM_SIZ_8b)) {
            outputColor = ANY8toIA8(inputColor, inputCoord, coord);
        }
        else if ((gConstants.srcFmt == G_IM_FMT_RGBA) && (gConstants.srcSiz == G_IM_SIZ_16b) && (gConstants.dstFmt == G_IM_FMT_IA) && (gConstants.dstSiz == G_IM_SIZ_16b)) {
            outputColor = RGBA16toIA16(inputColor, inputCoord, coord);
        }
        else {
            outputColor = inputColor;
        }

        gOutput[coord] = outputColor;
    }
}