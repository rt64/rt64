//
// RT64
//

#pragma once

// Color conversion formulas sourced from Tharo.

static const uint DitherPatternBayer[16] = {
    0, 4, 1, 5,
    4, 0, 5, 1,
    3, 7, 2, 6,
    7, 3, 6, 2
};

static const uint DitherPatternMagicSquare[16] = {
    0, 6, 1, 7,
    4, 2, 5, 3,
    3, 5, 2, 4,
    7, 1, 6, 0
};

uint DitherPatternIndex(uint2 coord) {
    return ((coord.y & 3) << 2) + (coord.x & 3);
}

uint DitherPatternValue(uint pattern, uint2 coord, uint randomSeed) {
    switch (pattern) {
    case 0: // MAGICSQ
        return DitherPatternMagicSquare[DitherPatternIndex(coord)];
    case 1: // BAYER
        return DitherPatternBayer[DitherPatternIndex(coord)];
    case 2: // NOISE
        return randomSeed & 7;
    case 3: // DISABLE
    default:
        return 0;
    }
}

uint AlphaDitherValue(uint colorDither, uint alphaDither, uint2 coord, uint randomSeed) {
    // Only the first bit of color dither is used here for pattern selection.
    switch (alphaDither) {
    case 0: // PATTERN
        return DitherPatternValue(colorDither & 1, coord, randomSeed);
    case 1: // NOTPATTERN
        return (~DitherPatternValue(colorDither & 1, coord, randomSeed)) & 7;
    case 2: // NOISE
        return randomSeed & 7;
    case 3: // DISABLE
    default:
        return 0;
    }
}

float4 I4ToFloat4(uint i4) {
    uint i = (i4 << 4) | i4;
    return i / 255.0f;
}

float4 IA4ToFloat4(uint ia4) {
    uint i = ia4 & 0b1110;
    i = (i << 4) | (i << 1) | (i >> 2);
    return float4(i / 255.0f, i / 255.0f, i / 255.0f, (ia4 & 1) ? 1.0f : 0.0f);
}

uint FloatToUINT8(float i) {
    return round(clamp(i, 0.0f, 1.0f) * 255.0f);
}

float4 I8ToFloat4(uint i) {
    return i / 255.0f;
}

float4 IA8ToFloat4(uint ia8) {
    uint i = (ia8 >> 4) & 0xF;
    uint a = (ia8 >> 0) & 0xF;
    i = (i << 4) | i;
    a = (a << 4) | a;
    return float4(i / 255.0f, i / 255.0f, i / 255.0f, a / 255.0f);
}

float4 RGBA16ToFloat4(uint rgba16) {
    uint r = (rgba16 >> 11) & 0x1F;
    uint g = (rgba16 >> 6) & 0x1F;
    uint b = (rgba16 >> 1) & 0x1F;
    return float4(
        ((r << 3) | (r >> 2)) / 255.0f,
        ((g << 3) | (g >> 2)) / 255.0f,
        ((b << 3) | (b >> 2)) / 255.0f,
        (rgba16 & 1) ? 1.0f : 0.0f
    );
}

uint Float4ToRGBA16(float4 i, uint dither, bool usesHDR) {
    const float cvgRange = usesHDR ? 65535.0f : 255.0f;
    uint r = round(clamp(i.r * 255.0f, 0.0f, 255.0f));
    uint g = round(clamp(i.g * 255.0f, 0.0f, 255.0f));
    uint b = round(clamp(i.b * 255.0f, 0.0f, 255.0f));
    int cvgModulo = round(i.a * cvgRange) % 8;
    uint a = (cvgModulo & 0x4) ? 1 : 0;
    r = min(r + dither, 255) >> 3;
    g = min(g + dither, 255) >> 3;
    b = min(b + dither, 255) >> 3;
    return (r << 11) | (g << 6) | (b << 1) | a;
}

float4 IA16ToFloat4(uint ia16) {
    uint i = (ia16 >> 8) & 0xFF;
    uint a = ia16 & 0xFF;
    return float4(i / 255.0f, i / 255.0f, i / 255.0f, a / 255.0f);
}

float4 RGBA32ToFloat4(uint i) {
    uint r = (i >> 24) & 0xFF;
    uint g = (i >> 16) & 0xFF;
    uint b = (i >> 8) & 0xFF;
    uint a = (i >> 0) & 0xFF;
    return float4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

uint Float4ToRGBA32(float4 i) {
    uint r = FloatToUINT8(i.r) << 24;
    uint g = FloatToUINT8(i.g) << 16;
    uint b = FloatToUINT8(i.b) << 8;
    uint a = FloatToUINT8(i.a) << 0;
    return (r | g | b | a);
}