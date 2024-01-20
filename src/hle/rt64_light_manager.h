//
// RT64
//

#pragma once

#include <vector>

#include "shared/rt64_point_light.h"

namespace RT64 {
    struct State;

    struct LightManager {
        struct Directional {
            hlslpp::float3 dir;
            hlslpp::float3 colTotal;
            float intensityTotal;
        };

        std::vector<Directional> directionalLights;
        std::vector<interop::PointLight> pointLights;
        hlslpp::float3 ambientColSum;
        int ambientSum;

        void reset();
        void processPointLight(State *state, const uint8_t lightIndex);
        void processDirLight(State *state, const uint8_t lightIndex);
        void processAmbientLight(State *state, const uint8_t lightIndex);
        void processLight(State *state, const uint8_t lightIndex, bool pointLightingEnabled);
        interop::PointLight estimatedSunLight(const float sunIntensity, const float sunDistance) const;
        hlslpp::float3 estimatedAmbientLight(const float ambientIntensity) const;
    };
};