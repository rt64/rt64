//
// RT64
//

#pragma once

namespace RT64 {
    struct GameConfiguration {
        float sunLightIntensity = 1.5f;
        float sunLightDistance = 25000.0f;
        bool estimateSunLight = false;
        bool rspLightAsDiffuse = false;
        float rspLightIntensity = 1.0f;
    };
};