//
// RT64
//

#include "rt64_projection.h"

namespace RT64 {
    // Projection

    void Projection::reset() {
        type = Type::None;
        used = false;
        gameCallCount = 0;
        pointLightCount = 0;
        transformsIndex = 0;
        scissorRect.reset();
        lightManager.reset();
    }

    void Projection::addGameCall(const GameCall &gameCall) {
        const int callIndex = gameCallCount++;
        adjustVector(gameCalls, gameCallCount);
        gameCalls[callIndex] = gameCall;

        if (!gameCall.callDesc.scissorRect.isNull()) {
            scissorRect.merge(gameCall.callDesc.scissorRect);
        }
    }

    void Projection::addPointLight(const interop::PointLight &light) {
        uint32_t lightIndex = pointLightCount++;
        adjustVector(pointLights, pointLightCount);
        pointLights[lightIndex] = light;
    }

    bool Projection::usesViewport() const {
        switch (type) {
        case Type::Perspective:
        case Type::Orthographic:
            return true;
        default:
            return false;
        }
    }
};