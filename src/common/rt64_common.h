//
// RT64
//

#pragma once

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "rt64_hlslpp.h"

#ifdef _WIN32
#   define DLLEXPORT extern "C" __declspec(dllexport)  
#   define CPPDLLEXPORT __declspec(dllexport)  
#else
#   define DLLEXPORT extern "C" __attribute__((visibility("default")))
#   define CPPDLLEXPORT __attribute__((visibility("default")))
#endif

namespace RT64 {
    enum class UpscaleMode {
        Bilinear,
        FSR,
        DLSS,
        XeSS
    };

    static const unsigned int DepthRayQueryMask = 0x1;
    static const unsigned int NoDepthRayQueryMask = 0x2;
    static const unsigned int ShadowCatcherRayQueryMask = 0x4;

    // Error string for last error or exception that was caught.
    extern std::string GlobalLastError;

#ifdef NDEBUG
#   define RT64_LOG_OPEN(x)
#   define RT64_LOG_CLOSE()
#   define RT64_LOG_PRINTF(x, ...)
#else
    extern FILE *GlobalLogFile;
#   ifdef _WIN32
#       define RT64_LOG_OPEN(x) do { RT64::GlobalLogFile = _wfopen(x, L"wt"); } while (0)
#   else
#       define RT64_LOG_OPEN(x) do { RT64::GlobalLogFile = fopen(x, "w"); } while (0)
#   endif
#   define RT64_LOG_CLOSE() do { fclose(RT64::GlobalLogFile); } while (0)
#   define RT64_LOG_PRINTF(x, ...) do { fprintf(RT64::GlobalLogFile, x, ## __VA_ARGS__); fprintf(RT64::GlobalLogFile, "\n"); fflush(RT64::GlobalLogFile); } while (0)
#   define RT64_LOG_PRINTF_DETAILED(x, ...) do { fprintf(RT64::GlobalLogFile, x, ## __VA_ARGS__); fprintf(RT64::GlobalLogFile, " (%s in %s:%d)\n", __FUNCTION__, __FILE__, __LINE__); fflush(RT64::GlobalLogFile); } while (0)
#endif

    inline void CalculateTextureRowWidthPadding(uint32_t rowPitch, uint32_t &rowWidth, uint32_t &rowPadding) {
        const int RowMultiple = 256;
        rowWidth = rowPitch;
        rowPadding = (rowWidth % RowMultiple) ? RowMultiple - (rowWidth % RowMultiple) : 0;
        rowWidth += rowPadding;
    }

    inline float HaltonSequence(int i, int b) {
        float f = 1.0;
        float r = 0.0;
        while (i > 0) {
            f = f / float(b);
            r = r + f * float(i % b);
            i = i / b;
        }

        return r;
    }

    inline hlslpp::float2 HaltonJitter(int frame, int phases) {
        return { HaltonSequence(frame % phases + 1, 2) - 0.5f, HaltonSequence(frame % phases + 1, 3) - 0.5f };
    }

    struct RectI {
        int x, y, w, h;
    };

    struct FixedRect {
        int32_t ulx;
        int32_t uly;
        int32_t lrx;
        int32_t lry;

        FixedRect();
        FixedRect(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry);
        void reset();
        bool isEmpty() const;
        bool isNull() const;
        void merge(const FixedRect &rect);
        FixedRect scaled(float x, float y) const;

        // Intersections can result in invalid rects if they don't overlap. Check if they're not null after using this.
        FixedRect intersection(const FixedRect &rect) const;
        bool contains(int32_t x, int32_t y) const;
        bool fullyInside(const FixedRect &rect) const;
        int32_t left(bool ceil) const;
        int32_t top(bool ceil) const;
        int32_t right(bool ceil) const;
        int32_t bottom(bool ceil) const;
        int32_t width(bool leftCeil, bool rightCeil) const;
        int32_t height(bool topCeil, bool bottomCeil) const;
    };

    struct FixedMatrix {
        int16_t integer[4][4];
        uint16_t frac[4][4];

        float toFloat(uint32_t i, uint32_t j) const;
        hlslpp::float4x4 toMatrix4x4() const;

        static float fixedToFloat(int16_t integerValue, uint16_t fracValue);
        static void modifyMatrix4x4Integer(hlslpp::float4x4 &matrix, uint32_t i, uint32_t j, int16_t integerValue);
        static void modifyMatrix4x4Fraction(hlslpp::float4x4 &matrix, uint32_t i, uint32_t j, uint16_t fracValue);
    };

    template<class T>
    void adjustVector(std::vector<T> &vector, size_t expectedSize) {
        if (vector.capacity() < expectedSize) {
            vector.reserve(expectedSize * 2);
        }

        if (vector.size() < expectedSize) {
            vector.resize(expectedSize);
        }
    }
};