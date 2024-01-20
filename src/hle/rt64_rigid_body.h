//
// RT64
//

#pragma once

#include "common/rt64_common.h"
#include "common/rt64_math.h"

namespace RT64 {
    struct RigidBody {
        DecomposedTransform transforms[2];
        hlslpp::float3 linearVelocity = {};
        float angularVelocity = 0.0f;
        uint8_t transformIndex = 0;
        bool lerpTranslation = false;
        bool lerpRotation = false;
        bool lerpScale = false;
        bool lerpSkew = false;
        bool lerpPerspective = false;
        bool lerpDecompose = true;

        // Default constructor.
        RigidBody();

        // Updates the vectors with the data from the transforms and updates the rigid body to determine if translation interpolation should be skipped.
        void updateLinear(const hlslpp::float4x4 &prevTransform, const hlslpp::float4x4 &curTransform, uint8_t componentInterpolation);

        // Updates the vectors with the data from the transforms and updates the rigid body to determine if rotation, scale, and skew interpolation should be skipped.
        void updateAngular(const hlslpp::float4x4 &prevTransform, const hlslpp::float4x4 &curTransform, uint8_t rotInterpolation, uint8_t scaleInterpolation, uint8_t skewInterpolation);

        // Determines if the perspective components of the transforms should be interpolated.
        void updatePerspective(const hlslpp::float4x4 &prevTransform, const hlslpp::float4x4 &curTransform, uint8_t perspInterpolation);

        // Decomposes the given matrix if specified and updates the tracked decomposed values.
        void updateDecomposition(const hlslpp::float4x4 &curTransform, bool decompose);

        // Lerps between the previous and current transform with the given weight and recomposes the result into a matrix.
        // Falls back to the provided matrix if decomposition has failed for either transform.
        hlslpp::float4x4 lerp(float weight, const hlslpp::float4x4& fallbackPrev, const hlslpp::float4x4& fallbackCur, bool slerp) const;
    };
};