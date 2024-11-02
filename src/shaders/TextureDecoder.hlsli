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

#define loadTMEM(relativeAddress) implLoadTMEM(relativeAddress, RDP_TMEM_MASK8, 0x0, oddRow, address, stride, TMEM)
#define loadTMEMLower(relativeAddress) implLoadTMEM(relativeAddress, RDP_TMEM_MASK16, 0x0, oddRow, address, stride, TMEM)
#define loadTMEMUpper(relativeAddress) implLoadTMEM(relativeAddress, RDP_TMEM_MASK16, (RDP_TMEM_BYTES >> 1), oddRow, address, stride, TMEM)
#define loadTLUT(paletteAddress) TMEM.Load(uint2((paletteAddress) & RDP_TMEM_MASK8, 0))

float4 sampleTMEMIA4(uint pixelValue4bit) {
    return IA4ToFloat4(pixelValue4bit);
}

float4 sampleTMEMI4(uint pixelValue4bit) {
    return I4ToFloat4(pixelValue4bit);
}

float4 sampleTMEMCI4TLUT(uint pixelValue4bit, uint tlut, uint palette, Texture1D<uint> TMEM) {
    const uint paletteAddress = RDP_TMEM_PALETTE + (palette << 7) + ((pixelValue4bit) << 3);
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

float4 sampleTMEMCI8(uint pixelValue0, uint tlut, Texture1D<uint> TMEM) {
    const uint paletteAddress = RDP_TMEM_PALETTE + (pixelValue0 << 3);
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

float4 sampleTMEMRGBA16(uint pixelValue0, uint pixelValue1) {
    const uint col16 = pixelValue1 | (pixelValue0 << 8);
    return RGBA16ToFloat4(col16);
}

float4 sampleTMEMIA16(uint pixelValue0, uint pixelValue1) {
    const uint ia16 = pixelValue1 | (pixelValue0 << 8);
    return IA16ToFloat4(ia16);
}

float4 sampleTMEMI16(uint pixelValue0, uint pixelValue1) {
    // Not a real format. The observed hardware behavior is replicated here by decoding as IA and storing it as IAIA.
    const uint intensity = pixelValue0;
    const uint alpha = pixelValue1;
    return float4(intensity / 255.0f, alpha / 255.0f, intensity / 255.0f, alpha / 255.0f);
}

float4 sampleTMEMCI16(uint pixelValue0, uint tlut, Texture1D<uint> TMEM) {
    switch (tlut) {
    case G_TT_RGBA16:
    case G_TT_IA16:
    default:
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

float4 sampleTMEM16b(uint pixelValue0, uint pixelValue1, uint fmt) {
    switch (fmt) {
    case G_IM_FMT_RGBA:
        return sampleTMEMRGBA16(pixelValue0, pixelValue1);
    case G_IM_FMT_IA:
        return sampleTMEMIA16(pixelValue0, pixelValue1);
    case G_IM_FMT_CI:
    case G_IM_FMT_I:
        return sampleTMEMI16(pixelValue0, pixelValue1);
    case G_IM_FMT_YUV:
    default:
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

float4 sampleTMEMRGBA32(bool oddRow, uint pixelAddress, uint address, uint stride, Texture1D<uint> TMEM) {
    uint r = loadTMEMLower(pixelAddress);
    uint g = loadTMEMLower(pixelAddress + 1);
    uint b = loadTMEMUpper(pixelAddress);
    uint a = loadTMEMUpper(pixelAddress + 1);
    return RGBA32ToFloat4((r << 24) | (g << 16) | (b << 8) | a);
}

float4 sampleTMEMI32(bool oddRow, bool oddColumn, uint pixelAddress, uint pixelValue0, uint pixelValue1, uint address, uint stride, Texture1D<uint> TMEM) {
    // Not a real format. The observed hardware behavior is replicated here by decoding
    // as RG as RGRG on even columns and BA as BABA on odd columns.
    const uint r = pixelValue0;
    const uint g = pixelValue1;
    const uint b = loadTMEM(pixelAddress + 2);
    const uint a = loadTMEM(pixelAddress + 3);
    return select(oddColumn,
        float4(r / 255.0f, g / 255.0f, r / 255.0f, g / 255.0f),
        float4(b / 255.0f, a / 255.0f, b / 255.0f, a / 255.0f));
}

float4 sampleTMEMCI32(uint pixelValue0, uint tlut, Texture1D<uint> TMEM) {
    switch (tlut) {
    case G_TT_RGBA16:
    case G_TT_IA16:
    default:
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

float4 sampleTMEM32b(bool oddRow, bool oddColumn, uint pixelAddress, uint pixelValue0, uint pixelValue1, int2 texelInt, uint fmt, uint address, uint stride, Texture1D<uint> TMEM) {
    switch (fmt) {
    case G_IM_FMT_RGBA:
        return sampleTMEMRGBA32(oddRow, pixelAddress, address, stride, TMEM);
    case G_IM_FMT_CI:
    case G_IM_FMT_IA:
    case G_IM_FMT_I:
        return sampleTMEMI32(oddRow, oddColumn, pixelAddress, pixelValue0, pixelValue1, address, stride, TMEM);
    case G_IM_FMT_YUV:
    default:
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

float4 sampleTMEM(int2 texelInt, uint siz, uint fmt, uint address, uint stride, uint tlut, uint palette, Texture1D<uint> TMEM) {
    const bool oddRow = (texelInt.y & 1);
    const bool oddColumn = (texelInt.x & 1);
    // Determine the left shift to use to calculate the tmem address. Effectively log2 of the pixel stride in half-bytes.
    //   4-bit (siz 0) -> 0
    //   8-bit (siz 1) -> 1
    //   16-bit (siz 2) -> 2
    //   32-bit (siz 3) -> 3
    //   RGBA32 (siz 3) -> 2 (32-bit RGBA textures sample both halves of TMEM, so their pixel stride is only 16 bits).
    const uint tmemShift = select_uint(and(fmt == G_IM_FMT_RGBA, siz == G_IM_SIZ_32b), 2, siz);
    const uint pixelAddress = texelInt.y * stride + ((texelInt.x << tmemShift) >> 1);
    const uint pixelValue0 = loadTMEM(pixelAddress + 0);
    const uint pixelValue1 = loadTMEM(pixelAddress + 1);
    // Calculate value for 4-bit formats.
    const uint pixelShift = select_uint(oddColumn, 0, 4);
    const uint pixelValue4bit = (pixelValue0 >> pixelShift) & 0xF;
    if (tlut > 0) {
        switch (siz) {
        case G_IM_SIZ_4b:
            return sampleTMEMCI4TLUT(pixelValue4bit, tlut, palette, TMEM);
        case G_IM_SIZ_8b:
            return sampleTMEMCI8(pixelValue0, tlut, TMEM);
        case G_IM_SIZ_16b:
            return sampleTMEMCI16(pixelValue0, tlut, TMEM);
        case G_IM_SIZ_32b:
            return sampleTMEMCI32(pixelValue0, tlut, TMEM);
        default:
            return float4(0.0f, 0.0f, 0.0f, 1.0f);
        }
    }
    else {
        switch (siz) {
        case G_IM_SIZ_4b:
            return sampleTMEM4b(pixelValue4bit, fmt, palette);
        case G_IM_SIZ_8b:
            return sampleTMEM8b(pixelValue0, fmt);
        case G_IM_SIZ_16b:
            return sampleTMEM16b(pixelValue0, pixelValue1, fmt);
        case G_IM_SIZ_32b:
            return sampleTMEM32b(oddRow, oddColumn, pixelAddress, pixelValue0, pixelValue1, texelInt, fmt, address, stride, TMEM);
        default:
            return float4(0.0f, 0.0f, 0.0f, 1.0f);
        }
    }
}