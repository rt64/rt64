//
// RT64
//

#include "Constants.hlsli"
#include "Math.hlsli"

float3 getBlueNoise(Texture2D<float4> blueNoiseTexture, uint2 pixelPos, uint frameCount) {
    uint2 blueNoiseBase;
    uint blueNoiseFrame = frameCount % 64;
    blueNoiseBase.x = (blueNoiseFrame % 8) * 64;
    blueNoiseBase.y = (blueNoiseFrame / 8) * 64;
    return blueNoiseTexture.Load(uint3(blueNoiseBase + pixelPos % 64, 0)).rgb;
}

// Sourced from http://cwyman.org/code/dxrTutors/tutors/Tutor14/tutorial14.md.html

float3 getCosHemisphereSampleBlueNoise(Texture2D<float4> blueNoiseTexture, uint2 pixelPos, uint frameCount, float3 normal) {
    float2 randVal = getBlueNoise(blueNoiseTexture, pixelPos, frameCount).rg;

    // Cosine weighted hemisphere sample from RNG
    float3 bitangent = getPerpendicularVector(normal);
    float3 tangent = cross(bitangent, normal);
    float r = sqrt(randVal.x);
    float phi = 2.0f * M_PI * randVal.y;

    // Get our cosine-weighted hemisphere lobe sample direction
    return tangent * (r * cos(phi).x) + bitangent * (r * sin(phi)) + normal.xyz * sqrt(max(0.0, 1.0f - randVal.x));
}

// Sourced from http://cwyman.org/code/dxrTutors/tutors/Tutor14/tutorial14.md.html

float3 getGGXMicrofacet(Texture2D<float4> blueNoiseTexture, uint2 pixelPos, uint frameCount, float roughness, float3 normal) {
    float2 randVal = getBlueNoise(blueNoiseTexture, pixelPos, frameCount).rg;
    float3 bitangent = getPerpendicularVector(normal);
    float3 tangent = cross(bitangent, normal);
    float a2 = roughness * roughness;
    float cosThetaH = sqrt(max(0.0f, (1.0f - randVal.x) / ((a2 - 1.0f) * randVal.x + 1.0f)));
    float sinThetaH = sqrt(max(0.0f, 1.0f - cosThetaH * cosThetaH));
    float phiH = randVal.y * M_PI * 2.0f;
    return tangent * (sinThetaH * cos(phiH)) + bitangent * (sinThetaH * sin(phiH)) + normal * cosThetaH;
}