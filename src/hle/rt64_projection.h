//
// RT64
//

#pragma once

#include "shared/rt64_point_light.h"

#include "rt64_game_call.h"
#include "rt64_light_manager.h"

namespace RT64 {
    struct Projection {
        enum class Type {
            None,
            Perspective,
            Orthographic,
            Rectangle,
            Triangle
        };

        Type type;
        uint32_t transformsIndex;
        std::vector<GameCall> gameCalls;
        uint32_t gameCallCount;
        LightManager lightManager;
        std::vector<interop::PointLight> pointLights;
        uint32_t pointLightCount;
        FixedRect scissorRect;

        union {
            struct {
                uint16_t screenAreaLeft;
                uint16_t screenAreaTop;
                uint16_t screenAreaRight;
                uint16_t screenAreaBottom;
            };

            uint64_t screenAreaBounds;
        };

        bool used;

        void reset();
        void addGameCall(const GameCall &gameCall);
        void addPointLight(const interop::PointLight &light);
        bool usesViewport() const;
    };
};