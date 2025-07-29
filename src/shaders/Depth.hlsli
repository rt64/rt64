//
// RT64
//

#pragma once

#define DEPTH_EXPONENT_MASK     0xE000
#define DEPTH_MANTISSA_MASK     0x1FFC
#define DEPTH_EXPONENT_SHIFT    13
#define DEPTH_MANTISSA_SHIFT    2

// Convert to 15.3 bit fixed-point, which is what the RSP uses.
uint FloatToFixedDepth(float i) {
    return round(i * (32768.0f * 8.0f - 1));
}

// Determine the exponent value based on the leading ones in the fixed-point depth value.
uint ExponentFromFixedDepth(uint depthFixed) {
    uint depthShifted = depthFixed << 14;
    int firstZero = firstbithigh(~depthShifted);
    return (uint)(clamp(31 - firstZero, 0, 7));
}

uint FloatToDepth16(float z, float dz) {
    uint zFixed = FloatToFixedDepth(z);
    uint exponent = ExponentFromFixedDepth(zFixed);

    // Determine the mantissa by shifting by the exponent. Cap the shift at 6 here, as an
    // exponent of 7 still only shifts by 6.
    uint mantissa = zFixed >> (6 - min(6, exponent));

    // Determine dz by finding the next largest power of two.
    uint dzFixed = FloatToFixedDepth(dz);
    dzFixed = clamp(dzFixed, 0x1, 0x8000);
    uint dzBit = firstbithigh(dzFixed);

    // Encode the two most significant bits in the visible bits.
    dzBit = (dzBit >> 2) & 0x3;

    // Pack dz, the exponent and mantissa into the floating point format.
    return dzBit | (exponent << DEPTH_EXPONENT_SHIFT) | ((mantissa << DEPTH_MANTISSA_SHIFT) & DEPTH_MANTISSA_MASK);
}

float Depth16ToFloat(uint i) {
    // Extract the exponent and mantissa from the depth buffer value.
    uint exponent = (i & DEPTH_EXPONENT_MASK) >> DEPTH_EXPONENT_SHIFT;
    uint mantissa = (i & DEPTH_MANTISSA_MASK) >> DEPTH_MANTISSA_SHIFT;

    // Convert the exponent and mantissa into a fixed-point value.
    uint shiftedMantissa = mantissa << (6 - min(6, exponent));
    uint mantissaBias = 0x40000U - (0x40000U >> exponent);

    return (shiftedMantissa + mantissaBias) / (32768.0f * 8.0f - 1);
}

float CoplanarDepthTolerance(float i) {
    uint depthFixed = FloatToFixedDepth(i);
    uint exponent = ExponentFromFixedDepth(depthFixed);
    const float MaxTolerance = 0.0005f;
    const uint MaxExponent = 3;
    return MaxTolerance / pow(2.0f, min(exponent, MaxExponent));
}

// Used for packing and unpacking back and forth from the color to the depth buffer at full precision.
// Only used if a path that does not rely on accurate writeback is detected.
// Sourced from https://skytiger.wordpress.com/2010/12/01/packing-depth-into-color/

float4 DepthToRGBA8888(float depth) {
    const float4 factor = float4(1.0f, 255.0f, 65025.0f, 16581375.0f);
    const float mask = 1.0f / 256.0f;
    float4 color = depth * factor;
    color.gba = frac(color.gba);
    color.rgb -= color.gba * mask;
    return color;
}

float RGBA8888ToDepth(float4 color) {
    const float4 factor = 1.0f / float4(1.0f, 255.0f, 65025.0f, 16581375.0f);
    return dot(color, factor);
}