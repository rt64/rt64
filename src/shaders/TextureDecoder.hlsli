//
// RT64
//

#pragma once

#include "Formats.hlsli"

#include "shared/rt64_hlsl.h"
#include "shared/rt64_f3d_defines.h"

#define RDP_TMEM_BYTES 0x1000
#define RDP_TMEM_PALETTE 0x800
#define RDP_TMEM_MASK8 0xFFF
#define RDP_TMEM_MASK16 0x7FF

uint implLoadTMEM(uint relativeAddress, uint maskAddress, uint orAddress, bool oddRow, uint textureStart, uint rowSize, Texture1D<uint> TMEM) {
    const uint rowStart = (relativeAddress / rowSize) * rowSize;
    const uint wordIndex = (relativeAddress - rowStart) / 4;
    const uint swapWordIndex = wordIndex ^ 1;
    const uint finalAddress = select_uint(oddRow,
        textureStart + rowStart + (swapWordIndex * 4) + (relativeAddress & 0x3),
        textureStart + relativeAddress);
    return TMEM.Load(int2(((finalAddress & maskAddress) | orAddress) & RDP_TMEM_MASK8, 0));
}

#define loadTMEMMasked(relativeAddress, mask, orAddress) implLoadTMEM(relativeAddress, mask, orAddress, oddRow, address, stride, TMEM)
#define loadTLUT(paletteAddress) TMEM.Load(uint2((paletteAddress) & RDP_TMEM_MASK8, 0))

float4 sampleTMEMIA4(uint pixelValue4bit) {
    return IA4ToFloat4(pixelValue4bit);
}

float4 sampleTMEMI4(uint pixelValue4bit) {
    return I4ToFloat4(pixelValue4bit);
}

float4 sampleTMEMCI4(uint pixelValue4bit, uint palette) {
    // Not a real format. Loads the palette index as the upper bits of the value.
    const uint paletteIndex = pixelValue4bit;
    const uint decodedValue = (palette << 4) | paletteIndex;
    return I8ToFloat4(decodedValue);
}

float4 sampleTMEM4b(uint pixelValue4bit, uint fmt, uint palette) {
    switch (fmt) {
    case G_IM_FMT_CI:
        return sampleTMEMCI4(pixelValue4bit, palette);
    case G_IM_FMT_IA:
        return sampleTMEMIA4(pixelValue4bit);
    case G_IM_FMT_I:
    case G_IM_FMT_RGBA: // Not a real format. Replicated by observing hardware behavior.
        return sampleTMEMI4(pixelValue4bit);
    case G_IM_FMT_YUV:
    default:
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

float4 sampleTMEMIA8(uint pixelValue0) {
    return IA8ToFloat4(pixelValue0);
}

float4 sampleTMEMI8(uint pixelValue0) {
    return I8ToFloat4(pixelValue0);
}

float4 sampleTMEM8b(uint pixelValue0, uint fmt) {
    switch (fmt) {
    case G_IM_FMT_IA:
        return sampleTMEMIA8(pixelValue0);
    case G_IM_FMT_I:
    case G_IM_FMT_RGBA: // Not a real format. Replicated by observing hardware behavior.
    case G_IM_FMT_CI: // CI behaves like I when a TLUT is not active.
        return sampleTMEMI8(pixelValue0);
    case G_IM_FMT_YUV:
    default:
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

float4 sampleTMEMRGBA16(uint pixelValue16bit) {
    const uint col16 = pixelValue16bit;
    return RGBA16ToFloat4(col16);
}

float4 sampleTMEMIA16(uint pixelValue16bit) {
    const uint ia16 = pixelValue16bit;
    return IA16ToFloat4(ia16);
}

float4 sampleTMEMI16(uint pixelValue0, uint pixelValue1) {
    // Not a real format. The observed hardware behavior is replicated here by decoding as IA and storing it as IAIA.
    const uint intensity = pixelValue0;
    const uint alpha = pixelValue1;
    return float4(intensity / 255.0f, alpha / 255.0f, intensity / 255.0f, alpha / 255.0f);
}

float4 sampleTMEM16b(uint pixelValue0, uint pixelValue1, uint fmt) {
    uint pixelValue16bit = pixelValue1 | (pixelValue0 << 8);
    switch (fmt) {
    case G_IM_FMT_RGBA:
        return sampleTMEMRGBA16(pixelValue16bit);
    case G_IM_FMT_IA:
        return sampleTMEMIA16(pixelValue16bit);
    case G_IM_FMT_CI:
    case G_IM_FMT_I:
        return sampleTMEMI16(pixelValue0, pixelValue1);
    case G_IM_FMT_YUV:
    default:
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

float4 sampleTMEMRGBA32(uint pixelValue0, uint pixelValue1, uint pixelValue2, uint pixelValue3) {
    uint r = pixelValue0;
    uint g = pixelValue1;
    uint b = pixelValue2;
    uint a = pixelValue3;
    return RGBA32ToFloat4((r << 24) | (g << 16) | (b << 8) | a);
}

float4 sampleTMEMI32(bool oddColumn, uint pixelValue0, uint pixelValue1, uint pixelValue2, uint pixelValue3) {
    // Not a real format. The observed hardware behavior is replicated here by decoding
    // as RG as RGRG on even columns and BA as BABA on odd columns.
    const uint r = pixelValue0;
    const uint g = pixelValue1;
    const uint b = pixelValue2;
    const uint a = pixelValue3;
    return select(oddColumn,
        float4(r / 255.0f, g / 255.0f, r / 255.0f, g / 255.0f),
        float4(b / 255.0f, a / 255.0f, b / 255.0f, a / 255.0f));
}

float4 sampleTMEM32b(bool oddColumn, uint pixelValue0, uint pixelValue1, uint pixelValue2, uint pixelValue3, uint fmt) {
    switch (fmt) {
    case G_IM_FMT_RGBA:
        return sampleTMEMRGBA32(pixelValue0, pixelValue1, pixelValue2, pixelValue3);
    case G_IM_FMT_CI:
    case G_IM_FMT_IA:
    case G_IM_FMT_I:
        return sampleTMEMI32(oddColumn, pixelValue0, pixelValue1, pixelValue2, pixelValue3);
    case G_IM_FMT_YUV:
    default:
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

float4 sampleTMEM(int2 texelInt, uint siz, uint fmt, uint address, uint stride, uint tlut, uint palette, Texture1D<uint> TMEM) {
    const bool oddRow = (texelInt.y & 1);
    const bool oddColumn = (texelInt.x & 1);
    const bool isRgba32 = and(fmt == G_IM_FMT_RGBA, siz == G_IM_SIZ_32b);
    const bool usesTlut = tlut > 0;
    // Determine the left shift to use to calculate the TMEM address. Effectively log2 of the pixel stride in half-bytes.
    //   4-bit (siz 0) -> 0
    //   8-bit (siz 1) -> 1
    //   16-bit (siz 2) -> 2
    //   32-bit (siz 3) -> 3
    //   RGBA32 (siz 3) -> 2 (32-bit RGBA textures sample both halves of TMEM, so their pixel stride is only 16 bits).
    const uint tmemShift = select_uint(isRgba32, 2, siz);

    // Determine the TMEM address mask. When using RGBA32 or TLUT, each sample only addresses half of TMEM.
    const uint addressMask = select_uint(or(isRgba32, usesTlut), RDP_TMEM_MASK16, RDP_TMEM_MASK8);

    // Load the two low samples for most formats.
    const uint pixelAddress = texelInt.y * stride + ((texelInt.x << tmemShift) >> 1);
    const uint pixelValue0 = loadTMEMMasked(pixelAddress + 0, addressMask, 0x0);
    const uint pixelValue1 = loadTMEMMasked(pixelAddress + 1, addressMask, 0x0);

    // Calculate value for 4-bit formats.
    const uint pixelShift = select_uint(oddColumn, 0, 4);
    const uint pixelValue4bit = (pixelValue0 >> pixelShift) & 0xF;

    if (usesTlut) {
        // Determine the palette index and load the value from the palette.
        const uint paletteAddress = select_uint(siz == G_IM_SIZ_4b,
            RDP_TMEM_PALETTE + (palette << 7) + ((pixelValue4bit) << 3),
            RDP_TMEM_PALETTE + (pixelValue0 << 3));
        const uint paletteValue = loadTLUT(paletteAddress + 1) | (loadTLUT(paletteAddress) << 8);
        switch (tlut) {
        case G_TT_RGBA16:
            return RGBA16ToFloat4(paletteValue);
        case G_TT_IA16:
            return IA16ToFloat4(paletteValue);
        default:
            return float4(0.0f, 0.0f, 0.0f, 1.0f);
        }
    }
    else {
        // Load the two high samples for 32-bit textures.
        const uint pixelAddress2 = select_uint(isRgba32, pixelAddress, pixelAddress + 2);
        const uint orAddress = select_uint(isRgba32, (RDP_TMEM_BYTES >> 1), 0);
        const uint pixelValue2 = loadTMEMMasked(pixelAddress2 + 0, addressMask, orAddress);
        const uint pixelValue3 = loadTMEMMasked(pixelAddress2 + 1, addressMask, orAddress);
        
        switch (siz) {
        case G_IM_SIZ_4b:
            return sampleTMEM4b(pixelValue4bit, fmt, palette);
        case G_IM_SIZ_8b:
            return sampleTMEM8b(pixelValue0, fmt);
        case G_IM_SIZ_16b:
            return sampleTMEM16b(pixelValue0, pixelValue1, fmt);
        case G_IM_SIZ_32b:
            return sampleTMEM32b(oddColumn, pixelValue0, pixelValue1, pixelValue2, pixelValue3, fmt);
        default:
            return float4(0.0f, 0.0f, 0.0f, 1.0f);
        }
    }
}