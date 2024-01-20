//
// RT64
//

#pragma once

#include "rt64_common.h"

namespace RT64 {
    inline constexpr uint32_t swappedOffset(uint32_t i) {
        return i ^ 3;
    }

    float sqr(float x);
    bool matrixIsNaN(const hlslpp::float4x4 &m);
    hlslpp::float4x4 matrixScale(float scale);
    hlslpp::float4x4 matrixScale(const hlslpp::float3& scale);
    void matrixDecomposeViewProj(const hlslpp::float4x4 &vp, hlslpp::float4x4 &v, hlslpp::float4x4 &p);
    hlslpp::float4x4 matrixTranslation(const hlslpp::float3 &t);
    hlslpp::float3x3 matrixRotationX(float rad);
    hlslpp::float3x3 matrixRotationY(float rad);
    hlslpp::float3x3 matrixRotationZ(float rad);
    hlslpp::float3x3 extract3x3(const hlslpp::float4x4 &m);
    hlslpp::float3x3 rotationFrom3x3(const hlslpp::float3x3 &m);
    float traceFrom3x3(const hlslpp::float3x3 &m);
    float matrixDifference(const hlslpp::float4x4 &a, const hlslpp::float4x4 &b);
    bool isMatrixAffine(const hlslpp::float4x4 &m);
    bool isMatrixIdentity(const hlslpp::float4x4 &m);
    bool isMatrixViewProj(const hlslpp::float4x4 &m);
    hlslpp::float4x4 lerpMatrix(const hlslpp::float4x4 &a, const hlslpp::float4x4 &b, float t);
    hlslpp::float4x4 lerpMatrix3x3(const hlslpp::float4x4 &a, const hlslpp::float4x4 &b, float t);
    hlslpp::float4x4 lerpMatrixComponents(const hlslpp::float4x4 &a, const hlslpp::float4x4 &b, bool linear, bool angular, bool perspective, float t);
    float nearPlaneFromProj(const hlslpp::float4x4 &m);
    float farPlaneFromProj(const hlslpp::float4x4 &m);
    float fovFromProj(const hlslpp::float4x4 &m);
    float pseudoRandom(uint32_t &s);
    hlslpp::float2 barycentricCoordinates(const hlslpp::float2 p, const hlslpp::float2 a, const hlslpp::float2 b, const hlslpp::float2 c);
    bool decomposeMatrix(const hlslpp::float4x4& mtx, hlslpp::quaternion& rotation, hlslpp::float3& scale, hlslpp::float3& skew,
        hlslpp::float3& translation, hlslpp::float4& perspective);
    hlslpp::float4x4 recomposeMatrix(const hlslpp::quaternion& rotation, const hlslpp::float3& scale, const hlslpp::float3& skew,
        const hlslpp::float3& translation, const hlslpp::float4& perspective);
    
    struct DecomposedTransform {
        hlslpp::quaternion rotation;
        hlslpp::float3 scale;
        hlslpp::float3 skew;
        hlslpp::float3 translation;
        hlslpp::float4 perspective;
        bool valid = false;

        DecomposedTransform() = default;
        DecomposedTransform(const hlslpp::float4x4& mtx);
    };

    DecomposedTransform lerpTransforms(const DecomposedTransform& a, const DecomposedTransform& b, float weight,
        bool lerpTranslation, bool lerpRotation, bool lerpScale, bool lerpSkew, bool lerpPerpsective, bool useSlerp);
};