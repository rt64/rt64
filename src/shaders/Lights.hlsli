//
// RT64
//

#include "BlueNoise.hlsli"
#include "Ray.hlsli"

#include "shared/rt64_extra_params.h"
#include "shared/rt64_point_light.h"

// Root signature

#define MAX_LIGHTS 24

float TraceShadow(RaytracingAccelerationStructure bvh, float3 rayOrigin, float3 rayDirection, float rayMinDist, float rayMaxDist, uint rayQueryMask) {
    RayDesc ray;
    ray.Origin = rayOrigin;
    ray.Direction = rayDirection;
    ray.TMin = rayMinDist;
    ray.TMax = rayMaxDist;

    RayDiff rayDiff;
    rayDiff.dOdx = float3(0.0f, 0.0f, 0.0f);
    rayDiff.dOdy = float3(0.0f, 0.0f, 0.0f);
    rayDiff.dDdx = float3(0.0f, 0.0f, 0.0f);
    rayDiff.dDdy = float3(0.0f, 0.0f, 0.0f);

    ShadowHitInfo shadowPayload;
    shadowPayload.shadowHit = 1.0f;
    shadowPayload.rayDiff = rayDiff;

    uint flags = RAY_FLAG_FORCE_NON_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;

#if SKIP_BACKFACE_SHADOWS == 1
    flags |= RAY_FLAG_CULL_BACK_FACING_TRIANGLES;
#endif

    TraceRay(bvh, flags, rayQueryMask, 1, 0, 1, ray, shadowPayload);
    return shadowPayload.shadowHit;
}

float CalculateLightIntensitySimple(PointLight pointLight, float3 position, float3 normal, float ignoreNormalFactor) {
    float3 lightPosition = pointLight.position;
    float lightRadius = pointLight.attenuationRadius;
    float lightAttenuation = pointLight.attenuationExponent;
    float lightDistance = length(position - lightPosition);
    float3 lightDirection = normalize(lightPosition - position);
    float NdotL = dot(normal, lightDirection);
    const float surfaceBiasDotOffset = 0.707106f;
    float surfaceBias = max(lerp(NdotL, 1.0f, ignoreNormalFactor) + surfaceBiasDotOffset, 0.0f);
    float sampleIntensityFactor = pow(max(1.0f - (lightDistance / lightRadius), 0.0f), lightAttenuation) * surfaceBias;
    return sampleIntensityFactor * dot(pointLight.diffuseColor, float3(1.0f, 1.0f, 1.0f));
}

// TODO: The implementation of this is mostly copied from the other function. Figure out a way to make
// both share the same overall implementation to compute the intensity.

float CalculateShadowIntensitySimple(PointLight pointLight, float3 position) {
    float3 lightPosition = pointLight.position;
    float lightRadius = pointLight.attenuationRadius;
    float lightAttenuation = pointLight.attenuationExponent;
    float lightDistance = length(position - lightPosition);
    return pow(max(1.0f - (lightDistance / lightRadius), 0.0f), lightAttenuation);
}

float3 ComputeLight(RaytracingAccelerationStructure bvh, PointLight pointLight, ExtraParams extraParams, uint2 launchIndex,
                    float3 rayDirection, float3 position, float3 normal, float3 specular, bool checkShadows, uint diSamples,
                    uint frameCount, Texture2D<float4> blueNoiseTexture) 
{
    float ignoreNormalFactor = extraParams.ignoreNormalFactor;
    float specularExponent = extraParams.specularExponent;
    float shadowRayBias = extraParams.shadowRayBias;
    float3 lightPosition = pointLight.position;
    float3 lightDirection = normalize(lightPosition - position);
    float3 lightSpotDirection = normalize(pointLight.direction);
    float lightSpotFalloffCosine = pointLight.spotFalloffCosine;
    float lightSpotMaxCosine = pointLight.spotMaxCosine;
    float lightRadius = pointLight.attenuationRadius;
    float lightAttenuation = pointLight.attenuationExponent;
    float lightPointRadius = (diSamples > 0) ? pointLight.pointRadius : 0.0f;
    float3 perpX = cross(-lightDirection, float3(0.f, 1.0f, 0.f));
    if (all(perpX == 0.0f)) {
        perpX.x = 1.0;
    }

    float3 perpY = cross(perpX, -lightDirection);
    float shadowOffset = pointLight.shadowOffset;
    const uint maxSamples = max(diSamples, 1);
    uint samples = maxSamples;
    float lLambertFactor = 0.0f;
    float3 lSpecularityFactor = 0.0f;
    float lShadowFactor = 0.0f;
    while (samples > 0) {
        float2 sampleCoordinate = getBlueNoise(blueNoiseTexture, launchIndex, frameCount + samples).rg * 2.0f - 1.0f;
        sampleCoordinate = normalize(sampleCoordinate) * saturate(length(sampleCoordinate));

        float3 samplePosition = lightPosition + perpX * sampleCoordinate.x * lightPointRadius + perpY * sampleCoordinate.y * lightPointRadius;
        float3 sampleDirection = normalize(samplePosition - position);
        float lightSpotDot = dot(sampleDirection, lightSpotDirection);
        if (lightSpotDot <= lightSpotMaxCosine) {
            float spotIntensity = 1.0f - clamp((lightSpotDot - lightSpotFalloffCosine) / (lightSpotMaxCosine - lightSpotFalloffCosine), 0.0f, 1.0f);
            float sampleDistance = length(position - samplePosition);
            float sampleIntensityFactor = pow(max(1.0f - (sampleDistance / lightRadius), 0.0f), lightAttenuation) * spotIntensity;
            float3 reflectedLight = reflect(-sampleDirection, normal);
            float NdotL = max(dot(normal, sampleDirection), 0.0f);
            float sampleLambertFactor = lerp(NdotL, 1.0f, ignoreNormalFactor) * sampleIntensityFactor;
            float sampleShadowFactor = 1.0f;
            if (checkShadows) {
                sampleShadowFactor = TraceShadow(bvh, position, sampleDirection, RAY_MIN_DISTANCE + shadowRayBias, (sampleDistance - shadowOffset), DEPTH_RAY_QUERY_MASK);
            }

            float3 sampleSpecularityFactor = specular * pow(max(saturate(dot(reflectedLight, -rayDirection) * sampleIntensityFactor), 0.0f), specularExponent);
            lLambertFactor += sampleLambertFactor / maxSamples;
            lSpecularityFactor += sampleSpecularityFactor / maxSamples;
            lShadowFactor += sampleShadowFactor / maxSamples;
        }

        samples--;
    }

    return (pointLight.diffuseColor * lLambertFactor + pointLight.specularColor * lSpecularityFactor) * lShadowFactor;
}

// TODO: The implementation of this is mostly copied from the other function. Figure out a way to make
// both share the same overall implementation to compute the samples.

float ComputeShadow(RaytracingAccelerationStructure bvh, PointLight pointLight, ExtraParams extraParams, uint2 launchIndex, float3 rayDirection,
                    float3 position, uint rayQueryMask, uint diSamples, uint frameCount, Texture2D<float4> blueNoiseTexture) 
{
    float shadowRayBias = extraParams.shadowRayBias;
    float3 lightPosition = pointLight.position;
    float3 lightDirection = normalize(lightPosition - position);
    float3 lightSpotDirection = normalize(pointLight.direction);
    float lightSpotFalloffCosine = pointLight.spotFalloffCosine;
    float lightSpotMaxCosine = pointLight.spotMaxCosine;
    float lightRadius = pointLight.attenuationRadius;
    float lightAttenuation = pointLight.attenuationExponent;
    float lightPointRadius = (diSamples > 0) ? pointLight.pointRadius : 0.0f;
    float3 perpX = cross(-lightDirection, float3(0.f, 1.0f, 0.f));
    if (all(perpX == 0.0f)) {
        perpX.x = 1.0;
    }

    float3 perpY = cross(perpX, -lightDirection);
    float shadowOffset = pointLight.shadowOffset;
    const uint maxSamples = max(diSamples, 1);
    uint samples = maxSamples;
    float lShadowFactor = 0.0f;
    while (samples > 0) {
        float2 sampleCoordinate = getBlueNoise(blueNoiseTexture, launchIndex, frameCount + samples).rg * 2.0f - 1.0f;
        sampleCoordinate = normalize(sampleCoordinate) * saturate(length(sampleCoordinate));

        float3 samplePosition = lightPosition + perpX * sampleCoordinate.x * lightPointRadius + perpY * sampleCoordinate.y * lightPointRadius;
        float3 sampleDirection = normalize(samplePosition - position);
        float lightSpotDot = dot(sampleDirection, lightSpotDirection);
        if (lightSpotDot <= lightSpotMaxCosine) {
            float spotIntensity = 1.0f - clamp((lightSpotDot - lightSpotFalloffCosine) / (lightSpotMaxCosine - lightSpotFalloffCosine), 0.0f, 1.0f);
            float sampleDistance = length(position - samplePosition);
            float sampleIntensityFactor = pow(max(1.0f - (sampleDistance / lightRadius), 0.0f), lightAttenuation) * spotIntensity;
            float sampleShadowFactor = 1.0f - TraceShadow(bvh, position, sampleDirection, RAY_MIN_DISTANCE + shadowRayBias, (sampleDistance - shadowOffset), rayQueryMask);
            lShadowFactor += sampleShadowFactor / maxSamples;
        }

        samples--;
    }

    return lShadowFactor;
}

float3 ComputeLightsRandom(RaytracingAccelerationStructure bvh, StructuredBuffer<PointLight> pointLights, uint pointLightsCount, ExtraParams extraParams, uint2 launchIndex, float3 rayDirection, float3 position, float3 normal,
                            float3 specular, uint maxLightCount, const bool checkShadows, uint diSamples, uint frameCount, Texture2D<float4> blueNoiseTexture) 
{
    float3 resultLight = float3(0.0f, 0.0f, 0.0f);
    uint lightGroupMaskBits = extraParams.lightGroupMaskBits;
    float ignoreNormalFactor = extraParams.ignoreNormalFactor;
    if (lightGroupMaskBits > 0) {
        uint sLightCount = 0;
        uint sLightIndices[MAX_LIGHTS + 1];
        float sLightIntensities[MAX_LIGHTS + 1];
        float totalLightIntensity = 0.0f;
        
        for (uint l = 0; (l < pointLightsCount) && (sLightCount < MAX_LIGHTS); l++) {
            if (lightGroupMaskBits & pointLights[l].groupBits) {
                float lightIntensity = CalculateLightIntensitySimple(pointLights[l], position, normal, ignoreNormalFactor);
                if (lightIntensity > EPSILON) {
                    sLightIntensities[sLightCount] = lightIntensity;
                    sLightIndices[sLightCount] = l;
                    totalLightIntensity += lightIntensity;
                    sLightCount++;
                }
            }
        }

        float randomRange = totalLightIntensity;
        uint lLightCount = min(sLightCount, maxLightCount);

        // TODO: Probability is disabled when more than one light is sampled because it's not trivial to calculate
        // the probability of the dependent events without replacement. In any case, it is likely more won't be needed
        // when a temporally stable denoiser is implemented.
        bool useProbability = lLightCount == 1;
        for (uint s = 0; s < lLightCount; s++) {
            float r = getBlueNoise(blueNoiseTexture, launchIndex, frameCount + s).r * randomRange;
            uint chosen = 0;
            float rLightIntensity = sLightIntensities[chosen];
            while ((chosen < (sLightCount - 1)) && (r >= rLightIntensity)) {
                chosen++;
                rLightIntensity += sLightIntensities[chosen];
            }

            // Store and clear the light intensity from the array.
            float cLightIntensity = sLightIntensities[chosen];
            uint cLightIndex = sLightIndices[chosen];
            float invProbability = useProbability ? (randomRange / cLightIntensity) : 1.0f;
            sLightIntensities[chosen] = 0.0f;
            randomRange -= cLightIntensity;

            // Compute and add the light.
            resultLight += ComputeLight(bvh, pointLights[cLightIndex], extraParams, launchIndex, rayDirection, position, normal, specular, checkShadows, diSamples, frameCount, blueNoiseTexture) * invProbability;
        }
    }

    return resultLight;
}

// TODO: The implementation of this is mostly copied from the other function. Figure out a way to make
// both share the same overall implementation to search for lights.

float ComputeShadowsRandom(RaytracingAccelerationStructure bvh, StructuredBuffer<PointLight> pointLights, uint pointLightsCount, ExtraParams extraParams, uint2 launchIndex, float3 rayDirection, float3 position,
                            uint maxLightCount, uint rayQueryMask, uint diSamples, uint frameCount, Texture2D<float4> blueNoiseTexture)
{
    float resultShadow = 0.0f;
    uint lightGroupMaskBits = extraParams.lightGroupMaskBits;
    float ignoreNormalFactor = extraParams.ignoreNormalFactor;
    if (lightGroupMaskBits > 0) {
        uint sLightCount = 0;
        uint sLightIndices[MAX_LIGHTS + 1];
        float sLightIntensities[MAX_LIGHTS + 1];
        float totalLightIntensity = 0.0f;
        for (uint l = 0; (l < pointLightsCount) && (sLightCount < MAX_LIGHTS); l++) {
            if (lightGroupMaskBits & pointLights[l].groupBits) {
                float lightIntensity = CalculateShadowIntensitySimple(pointLights[l], position);
                if (lightIntensity > EPSILON) {
                    sLightIntensities[sLightCount] = lightIntensity;
                    sLightIndices[sLightCount] = l;
                    totalLightIntensity += lightIntensity;
                    sLightCount++;
                }
            }
        }

        float randomRange = totalLightIntensity;
        uint lLightCount = min(sLightCount, maxLightCount);

        // TODO: Probability is disabled when more than one light is sampled because it's not trivial to calculate
        // the probability of the dependent events without replacement. In any case, it is likely more won't be needed
        // when a temporally stable denoiser is implemented.
        bool useProbability = lLightCount == 1;
        for (uint s = 0; s < lLightCount; s++) {
            float r = getBlueNoise(blueNoiseTexture, launchIndex, frameCount + s).r * randomRange;
            uint chosen = 0;
            float rLightIntensity = sLightIntensities[chosen];
            while ((chosen < (sLightCount - 1)) && (r >= rLightIntensity)) {
                chosen++;
                rLightIntensity += sLightIntensities[chosen];
            }

            // Store and clear the light intensity from the array.
            float cLightIntensity = sLightIntensities[chosen];
            uint cLightIndex = sLightIndices[chosen];
            float invProbability = useProbability ? (randomRange / cLightIntensity) : 1.0f;
            sLightIntensities[chosen] = 0.0f;
            randomRange -= cLightIntensity;

            // Compute and add the shadow.
            resultShadow += ComputeShadow(bvh, pointLights[cLightIndex], extraParams, launchIndex, rayDirection, position, rayQueryMask, diSamples, frameCount, blueNoiseTexture) * invProbability;
        }
    }

    return clamp(resultShadow, 0.0f, 1.0f);
}