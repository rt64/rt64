//
// RT64
//

#pragma once

// Color conversion formulas sourced from Tharo.

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

uint Float4ToRGBA16(float4 i) {
    uint r = round(clamp(i.r * 255.0f, 0.0f, 255.0f));
    uint g = round(clamp(i.g * 255.0f, 0.0f, 255.0f));
    uint b = round(clamp(i.b * 255.0f, 0.0f, 255.0f));
	int cvgModulo = round(i.a * 255.0f) % 8;
	uint a = (cvgModulo & 0x4) ? 1 : 0;
    r = r >> 3;
    g = g >> 3;
    b = b >> 3;
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