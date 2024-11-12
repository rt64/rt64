//
// RT64
//

#pragma once

#ifdef HLSL_CPU

#include <stdint.h>

#include "common/rt64_hlslpp.h"

#define FLOAT4X4_IDENTITY float4x4(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f)

namespace interop {
    typedef uint32_t uint;

    // These types do not have the same alignment in HLSLPP as HLSL.
    // We define them and auto-convert them wherever is possible.

    struct int1 {
        union {
            struct {
                int x;
            };

            int i32[1];
        };

        int1() = default;
        inline int1(int x) : x(x) { }
        inline int &operator[](int i) { return i32[i]; }
        inline const int &operator[](int i) const { return i32[i]; }
    };

    struct int2 {
        union {
            struct {
                int x;
                int y;
            };

            int i32[2];
        };

        int2() = default;
        inline int2(int x, int y) : x(x), y(y) { }
        inline int &operator[](int i) { return i32[i]; }
        inline const int &operator[](int i) const { return i32[i]; }
    };

    struct int3 {
        union {
            struct {
                int x;
                int y;
                int z;
            };

            int i32[3];
        };

        int3() = default;
        inline int3(int x, int y, int z) : x(x), y(y), z(z) { }
        inline int &operator[](int i) { return i32[i]; }
        inline const int &operator[](int i) const { return i32[i]; }
    };

    struct int4 {
        union {
            struct {
                int x;
                int y;
                int z;
                int w;
            };

            int i32[4];
        };

        int4() = default;
        inline int4(int x, int y, int z, int w) : x(x), y(y), z(z), w(w) { }
        inline int &operator[](int i) { return i32[i]; }
        inline const int &operator[](int i) const { return i32[i]; }
    };

    struct uint1 {
        union {
            struct {
                uint x;
            };

            uint u32[1];
        };

        uint1() = default;
        inline uint1(uint x) : x(x) { }
        inline uint &operator[](int i) { return u32[i]; }
        inline const uint &operator[](int i) const { return u32[i]; }
    };

    struct uint2 {
        union {
            struct {
                uint x;
                uint y;
            };

            uint u32[2];
        };

        uint2() = default;
        inline uint2(uint x, uint y) : x(x), y(y) { }
        inline uint &operator[](int i) { return u32[i]; }
        inline const uint &operator[](int i) const { return u32[i]; }
    };

    struct uint3 {
        union {
            struct {
                uint x;
                uint y;
                uint z;
            };

            uint u32[3];
        };

        uint3() = default;
        inline uint3(uint x, uint y, uint z) : x(x), y(y), z(z) { }
        inline uint &operator[](int i) { return u32[i]; }
        inline const uint &operator[](int i) const { return u32[i]; }
    };

    struct uint4 {
        union {
            struct {
                uint x;
                uint y;
                uint z;
                uint w;
            };

            uint u32[4];
        };

        uint4() = default;
        inline uint4(uint x, uint y, uint z, uint w) : x(x), y(y), z(z), w(w) { }
        inline uint &operator[](int i) { return u32[i]; }
        inline const uint &operator[](int i) const { return u32[i]; }
    };

    struct float1 {
        union {
            struct {
                float x;
            };

            float f32[1];
        };

        float1() = default;
        inline float1(float x) : x(x) { }
        inline float1(const hlslpp::float1 &v) : x(v[0]) { }
        inline operator hlslpp::float1() const { return hlslpp::float1(x); };
        inline float &operator[](int i) { return f32[i]; }
        inline const float &operator[](int i) const { return f32[i]; }
    };

    struct float2 {
        union {
            struct {
                float x, y;
            };

            float f32[2];
        };

        float2() = default;
        inline float2(float x, float y) : x(x), y(y) { }
        inline float2(const hlslpp::float2 &v) : x(v[0]), y(v[1]) { }
        inline operator hlslpp::float2() const { return hlslpp::float2(x, y); };
        inline float &operator[](int i) { return f32[i]; }
        inline const float &operator[](int i) const { return f32[i]; }
    };

    struct float3 {
        union {
            struct {
                float x, y, z;
            };

            float f32[3];
        };

        float3() = default;
        inline float3(float x, float y, float z) : x(x), y(y), z(z) { }
        inline float3(const hlslpp::float3 &v) : x(v[0]), y(v[1]), z(v[2]) { }
        inline operator hlslpp::float3() const { return hlslpp::float3(x, y, z); };
        inline float &operator[](int i) { return f32[i]; }
        inline const float &operator[](int i) const { return f32[i]; }
    };

    struct float4 {
        union {
            struct {
                float x, y, z, w;
            };

            float f32[4];
        };

        float4() = default;
        inline float4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) { }
        inline float4(const hlslpp::float4 &v) : x(v[0]), y(v[1]), z(v[2]), w(v[3]) { }
        inline operator hlslpp::float4() const { return hlslpp::float4(x, y, z, w); };
        inline float &operator[](int i) { return f32[i]; }
        inline const float &operator[](int i) const { return f32[i]; }
    };

    struct float4x4 {
        float m[4][4];

        float4x4() = default;

        inline float4x4(float m00, float m01, float m02, float m03, float m10, float m11, float m12, float m13, float m20, float m21, float m22, float m23, float m30, float m31, float m32, float m33) {
            m[0][0] = m00; m[0][1] = m01; m[0][2] = m02; m[0][3] = m03;
            m[1][0] = m10; m[1][1] = m11; m[1][2] = m12; m[1][3] = m13;
            m[2][0] = m20; m[2][1] = m21; m[2][2] = m22; m[2][3] = m23;
            m[3][0] = m30; m[3][1] = m31; m[3][2] = m32; m[3][3] = m33;
        }

        inline float4x4(const hlslpp::float4x4 &v) {
            m[0][0] = v[0][0]; m[0][1] = v[0][1]; m[0][2] = v[0][2]; m[0][3] = v[0][3];
            m[1][0] = v[1][0]; m[1][1] = v[1][1]; m[1][2] = v[1][2]; m[1][3] = v[1][3];
            m[2][0] = v[2][0]; m[2][1] = v[2][1]; m[2][2] = v[2][2]; m[2][3] = v[2][3];
            m[3][0] = v[3][0]; m[3][1] = v[3][1]; m[3][2] = v[3][2]; m[3][3] = v[3][3];
        }

        inline operator hlslpp::float4x4() const { return hlslpp::float4x4(
            m[0][0], m[0][1], m[0][2], m[0][3],
            m[1][0], m[1][1], m[1][2], m[1][3],
            m[2][0], m[2][1], m[2][2], m[2][3],
            m[3][0], m[3][1], m[3][2], m[3][3]); 
        };

        inline float* operator[](int i) { return m[i]; }
        inline const float *operator[](int i) const { return m[i]; }
        static float4x4 identity() { return FLOAT4X4_IDENTITY; }
    };

    // Wrappers for select to prevent implicit casting to float.
    inline uint select_uint(bool cond, uint val1, uint val2) {
        return cond ? val1 : val2;
    }

    inline int select_int(bool cond, int val1, int val2) {
        return cond ? val1 : val2;
    }
};


#define constmethod const

#else

#define constmethod

// Wrappers for select to prevent implicit casting to float.
uint select_uint(bool cond, uint val1, uint val2) {
    return select(cond, val1, val2);
}

int select_int(bool cond, int val1, int val2) {
    return select(cond, val1, val2);
}

#endif