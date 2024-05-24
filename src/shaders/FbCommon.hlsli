//
// RT64
//

#include "shared/rt64_f3d_defines.h"
#include "shared/rt64_fb_common.h"

#include "Formats.hlsli"

uint EndianSwapUINT16(uint i) {
    return ((i << 8) & 0xFF00) | ((i >> 8) & 0xFF);
}

// This endian swapping function generates a DXC-LLVM code generation bug when optimizations are enabled. All files that include this 
// function therefore need optimizations disabled to work until the issue is resolved.
//
// See this issue for more details: https://github.com/microsoft/DirectXShaderCompiler/issues/5104

uint EndianSwapUINT32(uint i) {
    return ((i << 24) & 0xFF000000) | ((i << 8) & 0xFF0000) | ((i >> 8) & 0xFF00) | ((i >> 24) & 0xFF);
}

uint EndianSwapUINT(uint i, uint siz) {
    switch (siz) {
    case G_IM_SIZ_4b:
        return i;
    case G_IM_SIZ_8b:
        return i;
    case G_IM_SIZ_16b:
        return EndianSwapUINT16(i);
    case G_IM_SIZ_32b:
        return EndianSwapUINT32(i);
    default:
        return 0;
    }
}

float4 UINT16ToFloat4(uint i, uint fmt) {
    switch (fmt) {
    case G_IM_FMT_RGBA:
        return RGBA16ToFloat4(i);
    case G_IM_FMT_CI:
        return 0.0f; // TODO
    case G_IM_FMT_IA:
        return 0.0f; // TODO
    case G_IM_FMT_I:
        return 0.0f; // TODO
    // Invalid format.
    default:
        return 0.0f;
    }
}

float4 UINT32ToFloat4(uint i, uint fmt) {
    switch (fmt) {
    case G_IM_FMT_RGBA:
        return RGBA32ToFloat4(i);
    case G_IM_FMT_CI:
        return 0.0f; // TODO
    case G_IM_FMT_IA:
        return 0.0f; // TODO
    case G_IM_FMT_I:
        return 0.0f; // TODO
    // Invalid format.
    default:
        return 0.0f;
    }
}

float4 UINTToFloat4(uint i, uint siz, uint fmt) {
    switch (siz) {
    // TODO
    case G_IM_SIZ_4b:
        return 0.0f;
    // TODO: It might be worth reviewing if it should unpack it the same way it packs it,
    // by alternating the red and green channels every column.
    case G_IM_SIZ_8b:
        return I8ToFloat4(i);
    case G_IM_SIZ_16b:
        return UINT16ToFloat4(i, fmt);
    case G_IM_SIZ_32b:
        return UINT32ToFloat4(i, fmt);
    // Invalid pixel size.
    default:
        return 0.0f;
    }
}

uint Float4ToUINT8(float4 i, uint fmt, bool oddColumn) {
    switch (fmt) {
    case G_IM_FMT_I:
    case G_IM_FMT_CI:
        return FloatToUINT8(oddColumn ? i.g : i.r);
    // TODO
    case G_IM_FMT_RGBA:
        return 0;
    // TODO
    case G_IM_FMT_IA:
        return 0;
    // Invalid format.
    default:
        return 0;
    }
}

uint Float4ToUINT16(float4 i, uint fmt, uint dither, bool usesHDR) {
    switch (fmt) {
    case G_IM_FMT_RGBA:
        return Float4ToRGBA16(i, dither, usesHDR);
    // TODO
    case G_IM_FMT_CI:
        return 0;
    // TODO
    case G_IM_FMT_IA:
        return 0;
    // TODO
    case G_IM_FMT_I:
        return 0;
    // Invalid format.
    default:
        return 0;
    }
}

uint Float4ToUINT32(float4 i, uint fmt) {
    switch (fmt) {
    case G_IM_FMT_RGBA:
        return Float4ToRGBA32(i);
    // TODO
    case G_IM_FMT_CI:
        return 0;
    // TODO
    case G_IM_FMT_IA:
        return 0;
    // TODO
    case G_IM_FMT_I:
        return 0;
    // Invalid format.
    default:
        return 0;
    }
}

uint Float4ToUINT(float4 i, uint siz, uint fmt, bool oddColumn, uint dither, bool usesHDR) {
    switch (siz) {
    // TODO
    case G_IM_SIZ_4b:
        return 0;
    case G_IM_SIZ_8b:
        return Float4ToUINT8(i, fmt, oddColumn);
    case G_IM_SIZ_16b:
        return Float4ToUINT16(i, fmt, dither, usesHDR);
    case G_IM_SIZ_32b:
        return Float4ToUINT32(i, fmt);
    // Invalid pixel size.
    default:
        return 0;
    }
}