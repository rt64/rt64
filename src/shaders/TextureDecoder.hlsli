//
// RT64
//

#pragma once

#include "Formats.hlsli"

#include "shared/rt64_f3d_defines.h"

#define RDP_TMEM_BYTES 0x1000
#define RDP_TMEM_PALETTE 0x800
#define RDP_TMEM_MASK8 0xFFF
#define RDP_TMEM_MASK16 0x7FF

uint implLoadTMEM(uint relativeAddress, uint maskAddress, uint orAddress, bool oddRow, uint textureStart, uint rowSize, Texture1D<uint> TMEM) {
    if (oddRow) {
        const uint rowStart = (relativeAddress / rowSize) * rowSize;
        const uint wordIndex = (relativeAddress - rowStart) / 4;
        const uint swapWordIndex = wordIndex ^ 1;
        const uint finalAddress = textureStart + rowStart + (swapWordIndex * 4) + (relativeAddress & 0x3);
        return TMEM.Load(int2(((finalAddress & maskAddress) | orAddress) & RDP_TMEM_MASK8, 0));
    }
    else {
        const uint finalAddress = textureStart + relativeAddress;
        return TMEM.Load(int2(((finalAddress & maskAddress) | orAddress) & RDP_TMEM_MASK8, 0));
    }
}

#define loadOddRow(yCoord) const bool oddRow = (yCoord & 1)
#define loadTMEM(relativeAddress) implLoadTMEM(relativeAddress, RDP_TMEM_MASK8, 0x0, oddRow, address, stride, TMEM)
#define loadTMEMLower(relativeAddress) implLoadTMEM(relativeAddress, RDP_TMEM_MASK16, 0x0, oddRow, address, stride, TMEM)
#define loadTMEMUpper(relativeAddress) implLoadTMEM(relativeAddress, RDP_TMEM_MASK16, (RDP_TMEM_BYTES >> 1), oddRow, address, stride, TMEM)
#define loadTLUT(paletteAddress) TMEM.Load(uint2((paletteAddress) & RDP_TMEM_MASK8, 0))

float4 sampleTMEMIA4(int2 texelInt, uint address, uint stride, Texture1D<uint> TMEM) {
    loadOddRow(texelInt.y);
    const uint pixelAddress = texelInt.y * stride + (texelInt.x / 2);
    const bool oddColumn = (texelInt.x & 1);
    const uint pixelValue = loadTMEM(pixelAddress);
    const uint pixelShift = oddColumn ? 0 : 4;
    return IA4ToFloat4((pixelValue >> pixelShift) & 0xF);
}

float4 sampleTMEMI4(int2 texelInt, uint address, uint stride, Texture1D<uint> TMEM) {
    loadOddRow(texelInt.y);
    const uint pixelAddress = texelInt.y * stride + (texelInt.x / 2);
    const bool oddColumn = (texelInt.x & 1);
    const uint pixelValue = loadTMEM(pixelAddress);
    const uint pixelShift = oddColumn ? 0 : 4;
    return I4ToFloat4((pixelValue >> pixelShift) & 0xF);
}

float4 sampleTMEMCI4TLUT(int2 texelInt, uint address, uint stride, uint tlut, uint palette, Texture1D<uint> TMEM) {
    loadOddRow(texelInt.y);
    const uint pixelAddress = texelInt.y * stride + (texelInt.x / 2);
    const bool oddColumn = (texelInt.x & 1);
    const uint pixelValue = loadTMEM(pixelAddress);
    const uint pixelShift = oddColumn ? 0 : 4;
    const uint paletteAddress = RDP_TMEM_PALETTE + (palette << 7) + (((pixelValue >> pixelShift) & 0xF) << 3);
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

float4 sampleTMEMCI4(int2 texelInt, uint address, uint stride, uint palette, Texture1D<uint> TMEM) {
    // Not a real format. Loads the palette index as the upper bits of the value.
    loadOddRow(texelInt.y);
    const uint pixelAddress = texelInt.y * stride + (texelInt.x / 2);
    const bool oddColumn = (texelInt.x & 1);
    const uint pixelValue = loadTMEM(pixelAddress);
    const uint pixelShift = oddColumn ? 0 : 4;
    const uint paletteIndex = (pixelValue >> pixelShift) & 0xF;
    const uint decodedValue = (palette << 4) | paletteIndex;
    return I8ToFloat4(decodedValue);
}

float4 sampleTMEM4b(int2 texelInt, uint fmt, uint address, uint stride, uint palette, Texture1D<uint> TMEM) {
    switch (fmt) {
    // Not a real format. Replicated by observing hardware behavior.
    case G_IM_FMT_RGBA:
        return sampleTMEMI4(texelInt, address, stride, TMEM);
    case G_IM_FMT_YUV:
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    case G_IM_FMT_CI:
        return sampleTMEMCI4(texelInt, address, stride, palette, TMEM);
    case G_IM_FMT_IA:
        return sampleTMEMIA4(texelInt, address, stride, TMEM);
    case G_IM_FMT_I:
        return sampleTMEMI4(texelInt, address, stride, TMEM);
    default:
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

float4 sampleTMEMIA8(int2 texelInt, uint address, uint stride, Texture1D<uint> TMEM) {
    loadOddRow(texelInt.y);
    const uint pixelAddress = texelInt.y * stride + texelInt.x;
    const uint pixelValue = loadTMEM(pixelAddress);
    return IA8ToFloat4(pixelValue);
}

float4 sampleTMEMI8(int2 texelInt, uint address, uint stride, Texture1D<uint> TMEM) {
    loadOddRow(texelInt.y);
    const uint pixelAddress = texelInt.y * stride + texelInt.x;
    const uint pixelValue = loadTMEM(pixelAddress);
    return I8ToFloat4(pixelValue);
}

float4 sampleTMEMCI8(int2 texelInt, uint address, uint stride, uint tlut, Texture1D<uint> TMEM) {
    loadOddRow(texelInt.y);
    const uint pixelAddress = texelInt.y * stride + texelInt.x;
    const uint pixelValue = loadTMEM(pixelAddress);
    const uint paletteAddress = RDP_TMEM_PALETTE + (pixelValue << 3);
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

float4 sampleTMEM8b(int2 texelInt, uint fmt, uint address, uint stride, Texture1D<uint> TMEM) {
    switch (fmt) {
        // Not a real format. Replicated by observing hardware behavior.
    case G_IM_FMT_RGBA:
        return sampleTMEMI8(texelInt, address, stride, TMEM);
    case G_IM_FMT_YUV:
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
        // CI behaves like I when a TLUT is not active.
    case G_IM_FMT_CI:
        return sampleTMEMI8(texelInt, address, stride, TMEM);
    case G_IM_FMT_IA:
        return sampleTMEMIA8(texelInt, address, stride, TMEM);
    case G_IM_FMT_I:
        return sampleTMEMI8(texelInt, address, stride, TMEM);
    default:
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

float4 sampleTMEMRGBA16(int2 texelInt, uint address, uint stride, Texture1D<uint> TMEM) {
    loadOddRow(texelInt.y);
    const uint pixelAddress = texelInt.y * stride + texelInt.x * 2;
    const uint col16 = loadTMEM(pixelAddress + 1) | (loadTMEM(pixelAddress) << 8);
    return RGBA16ToFloat4(col16);
}

float4 sampleTMEMIA16(int2 texelInt, uint address, uint stride, Texture1D<uint> TMEM) {
    loadOddRow(texelInt.y);
    const uint pixelAddress = texelInt.y * stride + texelInt.x * 2;
    const uint ia16 = loadTMEM(pixelAddress + 1) | (loadTMEM(pixelAddress) << 8);
    return IA16ToFloat4(ia16);
}

float4 sampleTMEMI16(int2 texelInt, uint address, uint stride, Texture1D<uint> TMEM) {
    // Not a real format. The observed hardware behavior is replicated here by decoding as IA and storing it as IAIA.
    loadOddRow(texelInt.y);
    const uint pixelAddress = texelInt.y * stride + texelInt.x * 2;
    const uint intensity = loadTMEM(pixelAddress);
    const uint alpha = loadTMEM(pixelAddress + 1);
    return float4(intensity / 255.0f, alpha / 255.0f, intensity / 255.0f, alpha / 255.0f);
}

float4 sampleTMEMCI16(int2 texelInt, uint address, uint stride, uint tlut, Texture1D<uint> TMEM) {
    switch (tlut) {
    case G_TT_RGBA16:
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    case G_TT_IA16:
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    default:
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

float4 sampleTMEM16b(int2 texelInt, uint fmt, uint address, uint stride, Texture1D<uint> TMEM) {
    switch (fmt) {
    case G_IM_FMT_RGBA:
        return sampleTMEMRGBA16(texelInt, address, stride, TMEM);
    case G_IM_FMT_YUV:
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    case G_IM_FMT_CI:
        return sampleTMEMI16(texelInt, address, stride, TMEM);
    case G_IM_FMT_IA:
        return sampleTMEMIA16(texelInt, address, stride, TMEM);
    case G_IM_FMT_I:
        return sampleTMEMI16(texelInt, address, stride, TMEM);
    default:
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

float4 sampleTMEMRGBA32(int2 texelInt, uint address, uint stride, Texture1D<uint> TMEM) {
    loadOddRow(texelInt.y);
    const uint pixelAddress = texelInt.y * stride + texelInt.x * 2;
    uint r = loadTMEMLower(pixelAddress);
    uint g = loadTMEMLower(pixelAddress + 1);
    uint b = loadTMEMUpper(pixelAddress);
    uint a = loadTMEMUpper(pixelAddress + 1);
    return RGBA32ToFloat4((r << 24) | (g << 16) | (b << 8) | a);
}

float4 sampleTMEMI32(int2 texelInt, uint address, uint stride, Texture1D<uint> TMEM) {
    // Not a real format. The observed hardware behavior is replicated here by decoding
    // as RG as RGRG on even columns and BA as BABA on odd columns.
    loadOddRow(texelInt.y);
    const bool oddColumn = (texelInt.x & 1);
    const uint pixelAddress = texelInt.y * stride + texelInt.x * 4;
    if (oddColumn) {
        uint r = loadTMEM(pixelAddress);
        uint g = loadTMEM(pixelAddress + 1);
        return float4(r / 255.0f, g / 255.0f, r / 255.0f, g / 255.0f);
    }
    else {
        uint b = loadTMEM(pixelAddress + 2);
        uint a = loadTMEM(pixelAddress + 3);
        return float4(b / 255.0f, a / 255.0f, b / 255.0f, a / 255.0f);
    }
}

float4 sampleTMEMCI32(int2 texelInt, uint address, uint stride, uint tlut, Texture1D<uint> TMEM) {
    switch (tlut) {
    case G_TT_RGBA16:
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    case G_TT_IA16:
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    default:
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

float4 sampleTMEM32b(int2 texelInt, uint fmt, uint address, uint stride, Texture1D<uint> TMEM) {
    switch (fmt) {
    case G_IM_FMT_RGBA:
        return sampleTMEMRGBA32(texelInt, address, stride, TMEM);
    case G_IM_FMT_YUV:
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    case G_IM_FMT_CI:
        return sampleTMEMI32(texelInt, address, stride, TMEM);
    case G_IM_FMT_IA:
        return sampleTMEMI32(texelInt, address, stride, TMEM);
    case G_IM_FMT_I:
        return sampleTMEMI32(texelInt, address, stride, TMEM);
    default:
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

float4 sampleTMEM(int2 texelInt, uint siz, uint fmt, uint address, uint stride, uint tlut, uint palette, Texture1D<uint> TMEM) {
    if (tlut > 0) {
        switch (siz) {
        case G_IM_SIZ_4b:
            return sampleTMEMCI4TLUT(texelInt, address, stride, tlut, palette, TMEM);
        case G_IM_SIZ_8b:
            return sampleTMEMCI8(texelInt, address, stride, tlut, TMEM);
        case G_IM_SIZ_16b:
            return sampleTMEMCI16(texelInt, address, stride, tlut, TMEM);
        case G_IM_SIZ_32b:
            return sampleTMEMCI32(texelInt, address, stride, tlut, TMEM);
        default:
            return float4(0.0f, 0.0f, 0.0f, 1.0f);
        }
    }
    else {
        switch (siz) {
        case G_IM_SIZ_4b:
            return sampleTMEM4b(texelInt, fmt, address, stride, palette, TMEM);
        case G_IM_SIZ_8b:
            return sampleTMEM8b(texelInt, fmt, address, stride, TMEM);
        case G_IM_SIZ_16b:
            return sampleTMEM16b(texelInt, fmt, address, stride, TMEM);
        case G_IM_SIZ_32b:
            return sampleTMEM32b(texelInt, fmt, address, stride, TMEM);
        default:
            return float4(0.0f, 0.0f, 0.0f, 1.0f);
        }
    }
}