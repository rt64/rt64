//
// RT64
//

#include "rt64_rigid_body.h"

#include "../include/rt64_extended_gbi.h"
#include "common/rt64_math.h"

namespace RT64 {
    // RigidBody

    RigidBody::RigidBody() {
        transforms[0] = {};
        transforms[1] = {};
        linearVelocity = { 0.0f, 0.0f, 0.0f };
    }

    void RigidBody::updateLinear(const hlslpp::float4x4 &prevTransform, const hlslpp::float4x4 &curTransform, uint8_t componentInterpolation) {
        if (componentInterpolation == G_EX_COMPONENT_AUTO) {
            const float Epsilon = 1e-6f;
            const float VelocityTolerance = 5.0f; // TODO: Make configurable.
            const float MagnitudeThreshold = 10.0f; // TODO: Make configurable.
            hlslpp::float3 prevPosition = prevTransform[3].xyz;
            hlslpp::float3 curPosition = curTransform[3].xyz;
            hlslpp::float3 curLinearVelocity = curPosition - prevPosition;
            hlslpp::float3 curAcceleration = (curLinearVelocity - linearVelocity);
            float prevVelMag = hlslpp::length(linearVelocity);
            float curVelMag = hlslpp::length(curLinearVelocity);
            float dotCurVel = std::max(hlslpp::dot(linearVelocity / std::max(prevVelMag, Epsilon), curLinearVelocity / std::max(curVelMag, Epsilon))[0], Epsilon);
            curVelMag /= dotCurVel;
            lerpTranslation = (curVelMag < VelocityTolerance) || (curVelMag / std::max(prevVelMag, Epsilon)) < MagnitudeThreshold;
            linearVelocity = curLinearVelocity;
        }
        else {
            lerpTranslation = (componentInterpolation == G_EX_COMPONENT_INTERPOLATE);
            linearVelocity = 0.0f;
        }
    }

    void RigidBody::updateAngular(const hlslpp::float4x4 &prevTransform, const hlslpp::float4x4 &curTransform, uint8_t rotInterpolation, uint8_t scaleInterpolation, uint8_t skewInterpolation) {
        // TODO independent scale and skew auto, currently assumed to match the result of rotation auto calculation.
        // If rotation isn't auto then these default to false for their auto settings.
        lerpScale = (scaleInterpolation == G_EX_COMPONENT_INTERPOLATE);
        lerpSkew = (skewInterpolation == G_EX_COMPONENT_INTERPOLATE);
        
        if (rotInterpolation == G_EX_COMPONENT_AUTO) {
            // Track angular velocity.
            const hlslpp::float3x3 invPrevRotation = hlslpp::inverse(rotationFrom3x3(extract3x3(prevTransform)));
            const hlslpp::float3x3 diffRotation = hlslpp::mul(invPrevRotation, rotationFrom3x3(extract3x3(curTransform)));
            float diffTrace = traceFrom3x3(diffRotation);
            float curAngularVelocity = std::acos((diffTrace - 1.0f) / 2.0f);
            angularVelocity = curAngularVelocity;

            // FIXME: Defaults to always interpolate.
            lerpRotation = true;

            // If scale or skew are also set to auto, use the result of rotation auto calculation for their value as well.
            if (scaleInterpolation == G_EX_COMPONENT_AUTO) {
                lerpScale = lerpRotation;
            }

            if (skewInterpolation == G_EX_COMPONENT_AUTO) {
                lerpSkew = lerpRotation;
            }
        }
        else {
            lerpRotation = (rotInterpolation == G_EX_COMPONENT_INTERPOLATE);
            angularVelocity = 0.0f;
        }
    }

    void RigidBody::updatePerspective(const hlslpp::float4x4 &prevTransform, const hlslpp::float4x4 &curTransform, uint8_t perspInterpolation) {
        // TODO auto perspective interpolation.
        lerpPerspective = (perspInterpolation == G_EX_COMPONENT_INTERPOLATE);
    }

    void RigidBody::updateDecomposition(const hlslpp::float4x4 &curTransform, bool decompose) {
        uint8_t newTransformIndex = transformIndex ^ 1;
        if (decompose) {
            transforms[newTransformIndex] = DecomposedTransform(curTransform);
        } else {
            transforms[newTransformIndex] = DecomposedTransform();
        }
        transformIndex = newTransformIndex;
        lerpDecompose = decompose;
    }

    
    hlslpp::float4x4 RigidBody::lerp(float weight, const hlslpp::float4x4& fallbackPrev, const hlslpp::float4x4& fallbackCur, bool slerp) const {
        // Return a linear component-wise interpolation of the fallback matrices if decomposition is disabled or if either decomposition is invalid.
        if (!lerpDecompose || !transforms[0].valid || !transforms[1].valid) {
            return lerpMatrixComponents(fallbackPrev, fallbackCur, lerpTranslation, lerpRotation, lerpPerspective, weight);
        }

        const DecomposedTransform& prevTransform = transforms[transformIndex ^ 1];
        const DecomposedTransform& curTransform = transforms[transformIndex];

        // Lerp the two transforms.
        DecomposedTransform lerpedTransform = lerpTransforms(prevTransform, curTransform, weight,
            lerpTranslation, lerpRotation, lerpScale, lerpSkew, lerpPerspective, slerp);
        // Compose a matrix from the resultant transform.
        return recomposeMatrix(lerpedTransform.rotation, lerpedTransform.scale, lerpedTransform.skew, lerpedTransform.translation, lerpedTransform.perspective);
    }
};