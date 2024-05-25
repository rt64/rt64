//
// RT64
//

#include "rt64_math.h"

#include <cassert>
#include <cmath>
#include <memory>

namespace RT64 {
    float sqr(float x) {
        return x * x;
    }

    bool matrixIsNaN(const hlslpp::float4x4 &m) {
        for (uint32_t i = 0; i < 4; i++) {
            if (hlslpp::any(hlslpp::isnan(m[i]))) {
                return true;
            }
        }

        return false;
    }

    hlslpp::float4x4 matrixScale(float scale) {
        hlslpp::float4x4 scaleMatrix(0.0f);
        scaleMatrix[0][0] = scale;
        scaleMatrix[1][1] = scale;
        scaleMatrix[2][2] = scale;
        scaleMatrix[3][3] = 1.0f;
        return scaleMatrix;
    }

    hlslpp::float4x4 matrixScale(const hlslpp::float3& scale) {
        hlslpp::float4x4 scaleMatrix(0.0f);
        scaleMatrix[0][0] = scale.x;
        scaleMatrix[1][1] = scale.y;
        scaleMatrix[2][2] = scale.z;
        scaleMatrix[3][3] = 1.0f;
        return scaleMatrix;
    }

    void matrixDecomposeViewProj(const hlslpp::float4x4 &vp, hlslpp::float4x4 &v, hlslpp::float4x4 &p) {
        v = hlslpp::float4x4::identity();
        p = hlslpp::float4x4::identity();

        p[2][3] = -1.0f;
        p[3][3] = 0.0f;
        v[0][2] = -vp[0][3];
        v[1][2] = -vp[1][3];
        v[2][2] = -vp[2][3];
        v[3][2] = -vp[3][3];

        p[2][2] = vp[0][2] / v[0][2];
        p[3][2] = vp[3][2] - p[2][2] * v[3][2];

        p[0][0] = sqrtf(sqr(vp[0][0]) + sqr(vp[1][0]) + sqr(vp[2][0]));
        p[1][1] = sqrtf(sqr(vp[0][1]) + sqr(vp[1][1]) + sqr(vp[2][1]));

        v[0][0] = vp[0][0] / p[0][0];
        v[1][0] = vp[1][0] / p[0][0];
        v[2][0] = vp[2][0] / p[0][0];
        v[3][0] = vp[3][0] / p[0][0];

        v[0][1] = vp[0][1] / p[1][1];
        v[1][1] = vp[1][1] / p[1][1];
        v[2][1] = vp[2][1] / p[1][1];
        v[3][1] = vp[3][1] / p[1][1];

        if (matrixIsNaN(v) || matrixIsNaN(p)) {
            v = hlslpp::float4x4::identity();
            p = vp;
        }
    }

    hlslpp::float4x4 matrixTranslation(const hlslpp::float3 &t) {
        hlslpp::float4x4 m = hlslpp::float4x4::identity();
        m[3].xyz = t;
        return m;
    }

    hlslpp::float3x3 matrixRotationX(float rad) {
        hlslpp::float3x3 m = hlslpp::float3x3::identity();
        const float rollCos = cos(rad);
        const float rollSin = sin(rad);
        m[0][0] = rollCos;
        m[0][1] = -rollSin;
        m[1][0] = rollSin;
        m[1][1] = rollCos;
        return m;
    }

    hlslpp::float3x3 matrixRotationY(float rad) {
        hlslpp::float3x3 m = hlslpp::float3x3::identity();
        const float pitchCos = cos(rad);
        const float pitchSin = sin(rad);
        m[0][0] = pitchCos;
        m[0][2] = pitchSin;
        m[2][0] = -pitchSin;
        m[2][2] = pitchCos;
        return m;
    }

    hlslpp::float3x3 matrixRotationZ(float rad) {
        hlslpp::float3x3 m = hlslpp::float3x3::identity();
        const float yawCos = cos(rad);
        const float yawSin = sin(rad);
        m[1][1] = yawCos;
        m[1][2] = -yawSin;
        m[2][1] = yawSin;
        m[2][2] = yawCos;
        return m;
    }

    hlslpp::float3x3 extract3x3(const hlslpp::float4x4 &m) {
        return hlslpp::float3x3(
            m[0][0], m[0][1], m[0][2],
            m[1][0], m[1][1], m[1][2],
            m[2][0], m[2][1], m[2][2]
        );
    }

    hlslpp::float3x3 rotationFrom3x3(const hlslpp::float3x3 &m) {
        return hlslpp::float3x3(hlslpp::normalize(m[0]), hlslpp::normalize(m[1]), hlslpp::normalize(m[2]));
    }

    float traceFrom3x3(const hlslpp::float3x3 &m) {
        return m[0][0] + m[1][1] + m[2][2];
    }

    float matrixDifference(const hlslpp::float4x4 &a, const hlslpp::float4x4 &b) {
        float difference = 0.0f;
        for (int i = 0; i < 4; i++) {
            difference += hlslpp::dot(hlslpp::abs(a[i] - b[i]), 1.0f);
        }

        return difference;
    }

    bool isMatrixAffine(const hlslpp::float4x4 &m) {
        return (m[0][3] == 0.0f) && (m[1][3] == 0.0f) && (m[2][3] == 0.0f) && (m[3][3] == 1.0f);
    }

    bool isMatrixIdentity(const hlslpp::float4x4 &m) {
        return hlslpp::all(m == hlslpp::float4x4::identity());
    }

    bool isMatrixViewProj(const hlslpp::float4x4 &m) {
        return (abs(m[3][3]) >= 1e-6f) && (abs(1.0f - m[3][3]) >= 1e-6f);
    }

    hlslpp::float4x4 lerpMatrix(const hlslpp::float4x4 &a, const hlslpp::float4x4 &b, float t) {
        // Copy b into the result.
        hlslpp::float4x4 c = b;

        // Replace with a component-wise linear interpolation between a and b.
        for (int i = 0; i < 4; i++) {
            c[i] = hlslpp::lerp(a[i], b[i], t);
        }

        return c;
    }

    hlslpp::float4x4 lerpMatrix3x3(const hlslpp::float4x4 &a, const hlslpp::float4x4 &b, float t) {
        // Copy b into the result.
        hlslpp::float4x4 c = b;
        
        // Replace the result's top left 3x3 with a component-wise linear interpolation between a and b.
        for (int i = 0; i < 3; i++) {
            c[i].xyz = hlslpp::lerp(a[i].xyz, b[i].xyz, t);
        }

        return c;
    }

    hlslpp::float4x4 lerpMatrixComponents(const hlslpp::float4x4 &a, const hlslpp::float4x4 &b, bool linear, bool angular, bool perspective, float t) {
        hlslpp::float4x4 ret;
        // Start by either component-wise lerping the top left 3x3 if rotation is enabled or directly copying it otherwise.
        // This leaves the last row and last column as a copy of b's in either case.
        if (angular) {
            ret = lerpMatrix3x3(a, b, t);
        }
        else {
            ret = b;
        }
        // Next, lerp the translation component of the last row if enabled, otherwise leave it intact from the initial copy step.
        if (linear) {
            ret[3].xyz = lerp(a[3].xyz, b[3].xyz, t);
        }
        // Finally, do the same for the last column if perspective is enabled.
        if (perspective) {
            ret[0].w = lerp(a[0].w, b[0].w, t);
            ret[1].w = lerp(a[1].w, b[1].w, t);
            ret[2].w = lerp(a[2].w, b[2].w, t);
            ret[3].w = lerp(a[3].w, b[3].w, t);
        }
        return ret;
    }

    float nearPlaneFromProj(const hlslpp::float4x4 &m) {
        return std::max((m[3][3] + m[3][2]) / (m[2][2] + m[2][3]), 1e-5f);
    }

    float farPlaneFromProj(const hlslpp::float4x4 &m) {
        return std::max((m[3][2] - m[3][3]) / (m[2][2] - m[2][3]), 1e-4f);
    }

    float fovFromProj(const hlslpp::float4x4 &m) {
        return std::max(2.0f * atanf(-m[2][3] / m[1][1]), 1e-2f);
    }

    float pseudoRandom(uint32_t &s) {
        s = 1664525u * s + 1013904223u;
        return float(s & 0x00FFFFFF) / float(0x01000000);
    }

    hlslpp::float2 barycentricCoordinates(const hlslpp::float2 p, const hlslpp::float2 a, const hlslpp::float2 b, const hlslpp::float2 c) {
        float area = -b.y * c.x + a.y * (c.x - b.x) + a.x * (b.y - c.y) + b.x * c.y;
        float s = (a.y * c.x - a.x * c.y + (c.y - a.y) * p.x + (a.x - c.x) * p.y) / area;
        float t = (a.x * b.y - a.y * b.x + (a.y - b.y) * p.x + (b.x - a.x) * p.y) / area;
        return { s, t };
    }

    bool epsilonEqual(float a, float b) {
        return abs(a - b) < std::numeric_limits<float>::epsilon();
    }
    
    // Adapted from https://github.com/g-truc/glm/blob/master/glm/gtx/matrix_decompose.inl
    /// Make a linear combination of two vectors and return the result.
    // result = (a * ascl) + (b * bscl)
    hlslpp::float3 vecCombine(
        const hlslpp::float3& a,
        const hlslpp::float3& b,
        float ascl, float bscl)
    {
        return (a * ascl) + (b * bscl);
    }

    hlslpp::float3 vecScale(const hlslpp::float3& v, float desiredLength)
    {
        return v * desiredLength / length(v);
    }

    bool decomposeMatrix(const hlslpp::float4x4& mtx, hlslpp::quaternion& rotation, hlslpp::float3& scale, hlslpp::float3& skew,
        hlslpp::float3& translation, hlslpp::float4& perspective)
    {
        hlslpp::float4x4 LocalMatrix(mtx);

        // Normalize the matrix.
        if(epsilonEqual(LocalMatrix[3][3], 0.0f)) {
            return false;
        }

        for(size_t i = 0; i < 4; i++) {
            for(size_t j = 0; j < 4; j++) {
                LocalMatrix[i][j] /= LocalMatrix[3][3];
            }
        }

        // perspectiveMatrix is used to solve for perspective, but it also provides
        // an easy way to test for singularity of the upper 3x3 component.
        hlslpp::float4x4 PerspectiveMatrix(LocalMatrix);

        for(size_t i = 0; i < 3; i++) {
            PerspectiveMatrix[i][3] = 0.0f;
        }
        PerspectiveMatrix[3][3] = 1.0f;

        /// TODO: Fixme!
        if(epsilonEqual(determinant(PerspectiveMatrix), 0.0f)) {
            return false;
        }

        // First, isolate perspective.  This is the messiest.
        if(
            !epsilonEqual(LocalMatrix[0][3], 0.0f) ||
            !epsilonEqual(LocalMatrix[1][3], 0.0f) ||
            !epsilonEqual(LocalMatrix[2][3], 0.0f))
        {
            // rightHandSide is the right hand side of the equation.
            hlslpp::float4 RightHandSide;
            RightHandSide[0] = LocalMatrix[0][3];
            RightHandSide[1] = LocalMatrix[1][3];
            RightHandSide[2] = LocalMatrix[2][3];
            RightHandSide[3] = LocalMatrix[3][3];

            // Solve the equation by inverting PerspectiveMatrix and multiplying
            // rightHandSide by the inverse.  (This is the easiest way, not
            // necessarily the best.)
            hlslpp::float4x4 InversePerspectiveMatrix = inverse(PerspectiveMatrix);
            hlslpp::float4x4 TransposedInversePerspectiveMatrix = transpose(InversePerspectiveMatrix);

            perspective = hlslpp::mul(TransposedInversePerspectiveMatrix, RightHandSide);

            // Clear the perspective partition
            LocalMatrix[0][3] = LocalMatrix[1][3] = LocalMatrix[2][3] = 0.0f;
            LocalMatrix[3][3] = 1.0f;
        }
        else
        {
            // No perspective.
            perspective = hlslpp::float4{0, 0, 0, 1.0f};
        }

        // Next take care of translation (easy).
        translation = hlslpp::float3(LocalMatrix[3].xyz);
        LocalMatrix[3] = hlslpp::float4(0, 0, 0, LocalMatrix[3].w);

        hlslpp::float3 Row[3], Pdum3;

        // Now get scale and shear.
        for(size_t i = 0; i < 3; ++i) {
            for(size_t j = 0; j < 3; ++j) {
                Row[i][j] = LocalMatrix[i][j];
            }
        }

        // Compute X scale factor and normalize first row.
        scale.x = length(Row[0]);// v3Length(Row[0]);

        Row[0] = vecScale(Row[0], 1.0f);

        // Compute XY shear factor and make 2nd row orthogonal to 1st.
        skew.z = dot(Row[0], Row[1]);
        Row[1] = vecCombine(Row[1], Row[0], 1.0f, -skew.z);

        // Now, compute Y scale and normalize 2nd row.
        scale.y = length(Row[1]);
        Row[1] = vecScale(Row[1], 1.0f);
        skew.z /= scale.y;

        // Compute XZ and YZ shears, orthogonalize 3rd row.
        skew.y = dot(Row[0], Row[2]);
        Row[2] = vecCombine(Row[2], Row[0], 1.0f, -skew.y);
        skew.x = dot(Row[1], Row[2]);
        Row[2] = vecCombine(Row[2], Row[1], 1.0f, -skew.x);

        // Next, get Z scale and normalize 3rd row.
        scale.z = length(Row[2]);
        Row[2] = vecScale(Row[2], 1.0f);
        skew.y /= scale.z;
        skew.x /= scale.z;

        // At this point, the matrix (in rows[]) is orthonormal.
        // Check for a coordinate system flip.  If the determinant
        // is -1, then negate the matrix and the scaling factors.
        Pdum3 = cross(Row[1], Row[2]);
        if(dot(Row[0], Pdum3).x < 0.0f) {
            for(size_t i = 0; i < 3; i++) {
                scale[i] *= -1.0f;
                Row[i] *= -1.0f;
            }
        }

        // Now, get the rotations out, as described in the gem.
        int i, j, k = 0;
        float root, trace = Row[0].x + Row[1].y + Row[2].z;
        if(trace > 0.0f)
        {
            root = sqrt(trace + 1.0f);
            rotation.w = 0.5f * root;
            root = 0.5f / root;
            rotation.x = root * (Row[1].z - Row[2].y);
            rotation.y = root * (Row[2].x - Row[0].z);
            rotation.z = root * (Row[0].y - Row[1].x);
        } // End if > 0
        else
        {
            static int Next[3] = {1, 2, 0};
            i = 0;
            if(Row[1].y > Row[0].x) i = 1;
            if(Row[2].z > Row[i][i]) i = 2;
            j = Next[i];
            k = Next[j];

            root = sqrt(Row[i][i] - Row[j][j] - Row[k][k] + 1.0f);

            rotation.f32[i] = 0.5f * root;
            root = 0.5f / root;
            rotation.f32[j] = root * (Row[i][j] + Row[j][i]);
            rotation.f32[k] = root * (Row[i][k] + Row[k][i]);
            rotation.w = root * (Row[j][k] - Row[k][j]);
        } // End if <= 0

        return true;
    }

    hlslpp::float4x4 recomposeMatrix(const hlslpp::quaternion& rotation, const hlslpp::float3& scale, const hlslpp::float3& skew,
        const hlslpp::float3& translation, const hlslpp::float4& perspective)
    {
        hlslpp::float4x4 m = hlslpp::float4x4::identity();

        m[0][3] = perspective.x;
        m[1][3] = perspective.y;
        m[2][3] = perspective.z;
        m[3][3] = perspective.w;

        m = mul(matrixTranslation(translation.xyz), m);
        m = mul(hlslpp::float4x4(rotation), m);

        if (fabs(skew.x) > 0.0f) {
            hlslpp::float4x4 tmp = hlslpp::float4x4::identity();
            tmp[2][1] = skew.x;
            m = mul(tmp, m);
        }

        if (fabs(skew.y) > 0.0f) {
            hlslpp::float4x4 tmp = hlslpp::float4x4::identity();
            tmp[2][0] = skew.y;
            m = mul(tmp, m);
        }

        if (fabs(skew.z) > 0.0f) {
            hlslpp::float4x4 tmp = hlslpp::float4x4::identity();
            tmp[1][0] = skew.z;
            m = mul(tmp, m);
        }

        m = mul(matrixScale(scale), m);

        return m;
    }

    DecomposedTransform::DecomposedTransform(const hlslpp::float4x4& mtx) {
        valid = decomposeMatrix(mtx, rotation, scale, skew, translation, perspective);
    }

    DecomposedTransform lerpTransforms(const DecomposedTransform& a, const DecomposedTransform& b, float weight,
        bool lerpTranslation, bool lerpRotation, bool lerpScale, bool lerpSkew, bool lerpPerpsective, bool useSlerp)
    {
        assert(a.valid && b.valid);
        DecomposedTransform ret;

        // Lerp the individual fields based on the provided flags.
        if (lerpTranslation) {
            ret.translation = lerp(a.translation, b.translation, weight);
        }
        else {
            ret.translation = b.translation;
        }

        if (lerpRotation) {
            if (float(dot(a.rotation, b.rotation)) > 0.0f) {
                if (useSlerp) {
                    ret.rotation = slerp(a.rotation, b.rotation, 1.0f - weight);
                }
                else {
                    ret.rotation = lerp(a.rotation, b.rotation, weight);
                }
            }
            else {
                if (useSlerp) {
                    ret.rotation = slerp(a.rotation, -b.rotation, 1.0f - weight);
                }
                else {
                    ret.rotation = lerp(a.rotation, -b.rotation, weight);
                }
            }
            ret.rotation = normalize(ret.rotation);
        }
        else {
            ret.rotation = b.rotation;
        }

        if (lerpScale) {
            ret.scale = lerp(a.scale, b.scale, weight);        
        }
        else {
            ret.scale = b.scale;
        }

        if (lerpSkew) {
            ret.skew = lerp(a.skew, b.skew, weight);
        }
        else {
            ret.skew = b.skew;
        }

        if (lerpPerpsective) {
            ret.perspective = lerp(a.perspective, b.perspective, weight);
        }
        else {
            ret.perspective = b.perspective;
        }

        // Mark the resultant transform as valid and return it.
        ret.valid = true;
        return ret;
    }
};