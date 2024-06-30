//
// RT64
//

#include "rt64_light_manager.h"

#include "common/rt64_math.h"

#include "rt64_state.h"

#define ENABLE_AUTOMATIC_POINT_LIGHTS 0

namespace RT64 {
    void LightManager::reset() {
        directionalLights.clear();
        pointLights.clear();
        ambientColSum = { 0.0f, 0.0f, 0.0f };
        ambientSum = 0;
    }

    void LightManager::processPointLight(State *state, const uint8_t lightIndex) {
#if ENABLE_AUTOMATIC_POINT_LIGHTS
        assert(false && "Check point lighting.");
        const uint8_t *data = state->rsp->lights[lightIndex].data;
        uint8_t colr = data[swappedOffset(0)];
        uint8_t colg = data[swappedOffset(1)];
        uint8_t colb = data[swappedOffset(2)];
        uint8_t kc = data[swappedOffset(3)];
        uint8_t kl = data[swappedOffset(7)];
        int16_t posx = (data[swappedOffset(8)] << 8) | data[swappedOffset(9)];
        int16_t posy = (data[swappedOffset(10)] << 8) | data[swappedOffset(11)];
        int16_t posz = (data[swappedOffset(12)] << 8) | data[swappedOffset(13)];
        uint8_t kq = data[swappedOffset(14)];

        PointLight light;
        light.position = { static_cast<float>(posx), static_cast<float>(posy), static_cast<float>(posz) };
        for (const PointLight &l : pointLights) {
            // A proximity lookup is not required here because the values are converted from the same shorts during runtime.
            if ((l.position.x == light.position.x) && (l.position.y == light.position.y) && (l.position.z == light.position.z)) {
                return;
            }
        }

        light.direction = { 0.0f, -1.0f, 0.0f };
        light.diffuseColor = { colr / 255.0f, colg / 255.0f , colb / 255.0f };
        light.attenuationRadius = 150.0f;
        light.pointRadius = 1.0f;
        light.spotFalloffCosine = 1.0f;
        light.spotMaxCosine = 1.0f;
        light.specularColor = { light.diffuseColor.x * 0.5f, light.diffuseColor.y * 0.5f, light.diffuseColor.z * 0.5f };
        light.shadowOffset = 40.0f;
        light.attenuationExponent = 1.0f;
        light.flickerIntensity = 0.0f;
        light.groupBits = 1;
        pointLights.push_back(light);
#endif
    }

    void LightManager::processDirLight(State *state, const uint8_t lightIndex) {
        const RSP::Light &light = state->rsp->lights[lightIndex];
        float colrf = light.dir.colr / 255.0f;
        float colgf = light.dir.colg / 255.0f;
        float colbf = light.dir.colb / 255.0f;
        float intensity = colrf * colrf + colgf * colgf + colbf * colbf;
        hlslpp::float3 dir;
        dir.x = (light.dir.dirx / 127.0f);
        dir.y = (light.dir.diry / 127.0f);
        dir.z = (light.dir.dirz / 127.0f);

        for (Directional &l : directionalLights) {
            if (hlslpp::all(l.dir == dir)) {
                l.colTotal.x += colrf * intensity;
                l.colTotal.y += colgf * intensity;
                l.colTotal.z += colbf * intensity;
                l.intensityTotal += intensity;
                return;
            }
        }

        Directional l;
        l.dir = dir;
        l.colTotal.x = colrf * intensity;
        l.colTotal.y = colgf * intensity;
        l.colTotal.z = colbf * intensity;
        l.intensityTotal = intensity;
        directionalLights.push_back(l);
    }

    void LightManager::processAmbientLight(State *state, const uint8_t lightIndex) {
        const RSP::Light &light = state->rsp->lights[lightIndex];
        ambientColSum.x += (light.dir.colr / 255.0f);
        ambientColSum.y += (light.dir.colg / 255.0f);
        ambientColSum.z += (light.dir.colb / 255.0f);
        ambientSum++;
    }
    
    void LightManager::processLight(State *state, const uint8_t lightIndex, bool pointLightingEnabled) {
        const RSP::Light &light = state->rsp->lights[lightIndex];
        if (pointLightingEnabled && (light.pos.kc > 0)) {
            processPointLight(state, lightIndex);
        }
        else {
            processDirLight(state, lightIndex);
        }
    }

    interop::PointLight LightManager::estimatedSunLight(const float sunIntensity, const float sunDistance) const {
        hlslpp::float3 sunDir = { 0.5f, 1.0f, 0.0f };
        hlslpp::float3 sunCol = { 0.8f, 0.7f, 0.6f };
        float biggestIntensity = 0;
        int biggestDirLight = -1;
        for (size_t i = 0; i < directionalLights.size(); i++) {
            if (directionalLights[i].intensityTotal > biggestIntensity) {
                biggestDirLight = i;
                biggestIntensity = directionalLights[i].intensityTotal;
            }
        }

        // Pick the light with the biggest amount of matches.
        if (biggestDirLight >= 0) {
            const auto &l = directionalLights[biggestDirLight];
            sunDir = l.dir;
            sunCol.x = l.colTotal.x / l.intensityTotal;
            sunCol.y = l.colTotal.y / l.intensityTotal;
            sunCol.z = l.colTotal.z / l.intensityTotal;
        }

        interop::PointLight res;
        float length = sqrtf(sunDir.x * sunDir.x + sunDir.y * sunDir.y + sunDir.z * sunDir.z);
        res.position.x = (sunDir.x / length) * sunDistance;
        res.position.y = (sunDir.y / length) * sunDistance;
        res.position.z = (sunDir.z / length) * sunDistance;
        res.direction.x = sunDir.x;
        res.direction.y = sunDir.y;
        res.direction.z = sunDir.z;
        res.diffuseColor.x = sunCol.x;
        res.diffuseColor.y = sunCol.y;
        res.diffuseColor.z = sunCol.z;
        res.diffuseColor.x *= sunIntensity;
        res.diffuseColor.y *= sunIntensity;
        res.diffuseColor.z *= sunIntensity;
        res.specularColor.x = res.diffuseColor.x * 0.5f;
        res.specularColor.y = res.diffuseColor.y * 0.5f;
        res.specularColor.z = res.diffuseColor.z * 0.5f;
        res.pointRadius = sunDistance * 0.01f;
        res.spotFalloffCosine = 1.0f;
        res.spotMaxCosine = 1.0f;
        res.attenuationRadius = 99999997952.0f;
        res.attenuationExponent = 1.0f;
        res.flickerIntensity = 0.0f;
        res.shadowOffset = 0.0f;
        res.groupBits = 1;
        return res;
    }

    hlslpp::float3 LightManager::estimatedAmbientLight(const float ambientIntensity) const {
        hlslpp::float3 estimate;
        estimate.x = ambientColSum.x / ambientSum;
        estimate.y = ambientColSum.y / ambientSum;
        estimate.z = ambientColSum.z / ambientSum;
        estimate.x *= ambientIntensity;
        estimate.y *= ambientIntensity;
        estimate.z *= ambientIntensity;
        return estimate;
    }
};