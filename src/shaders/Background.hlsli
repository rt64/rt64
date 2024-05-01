//
// RT64
//

#include "Math.hlsli"

float2 FakeEnvMapUV(float3 rayDirection, float yawOffset) {
    float yaw = fmod(yawOffset + atan2(rayDirection.x, -rayDirection.z) + M_PI, M_TWO_PI);
    float pitch = fmod(atan2(-rayDirection.y, sqrt(rayDirection.x * rayDirection.x + rayDirection.z * rayDirection.z)) + M_PI, M_TWO_PI);
    return float2(yaw / M_TWO_PI, pitch / M_TWO_PI);
}

float4 Sample2D(Texture2D tex, float2 screenUV, SamplerState clampSampler) {
    return tex.SampleLevel(clampSampler, screenUV, 0);
}

float4 SampleAsEnvMap(Texture2D tex, float3 rayDirection, SamplerState mirrorSampler) {
    return tex.SampleLevel(mirrorSampler, FakeEnvMapUV(rayDirection, 0.0f), 0);
}
