//
// RT64
//

#pragma once

#include <array>
#include <stdint.h>

#include "render/rt64_native_target.h"

#include "rt64_framebuffer_changes.h"
#include "rt64_framebuffer_storage.h"

namespace RT64 {
    struct RenderTarget;

    struct Framebuffer {
        enum class Type {
            None,
            Color,
            Depth
        };

        uint32_t addressStart;
        uint32_t addressEnd;
        uint8_t siz;
        uint32_t width;
        uint32_t height;
        uint32_t maxHeight;
        uint32_t readHeight;
        NativeTarget nativeTarget;
        std::vector<uint8_t> nativeSwappedRAM;
        FixedRect lastWriteRect;
        uint8_t lastWriteFmt;
        Type lastWriteType;
        uint64_t lastWriteTimestamp;
        uint32_t modifiedBytes;
        uint32_t RAMBytes;
        uint64_t RAMHash;
        std::array<uint32_t, 4> ditherPatterns;
        bool widthChanged;
        bool sizChanged;
        bool rdramChanged;
        bool interpolationEnabled;
        bool everUsedAsDepth;

        Framebuffer();
        ~Framebuffer();
        uint32_t imageRowBytes(uint32_t rowWidth) const;
        bool contains(uint32_t start, uint32_t end) const;
        bool overlaps(uint32_t start, uint32_t end) const;
        void discardLastWrite();
        bool isLastWriteDifferent(Framebuffer::Type newType) const;
        uint32_t copyRAMToNativeAndChanges(RenderWorker *worker, FramebufferChange &fbChange, const uint8_t *src, uint32_t rowStart, uint32_t rowCount, uint8_t fmt, bool invalidateTargets, const ShaderLibrary *shaderLibrary);
        FramebufferChange *readChangeFromBytes(RenderWorker *worker, FramebufferChangePool &fbChangePool, Type type, uint8_t fmt, const uint8_t *src, uint32_t rowStart, uint32_t rowCount, const ShaderLibrary *shaderLibrary);
        FramebufferChange *readChangeFromStorage(RenderWorker *worker, const FramebufferStorage &fbStorage, FramebufferChangePool &fbChangePool, Type type, uint8_t fmt,
            uint32_t maxFbPairIndex, uint32_t rowStart, uint32_t rowCount, const ShaderLibrary *shaderLibrary);

        void copyRenderTargetToNative(RenderWorker *worker, RenderTarget *target, uint32_t dstRowWidth, uint32_t dstRowStart, uint32_t dstRowEnd, uint8_t fmt, uint32_t ditherRandomSeed, const ShaderLibrary *shaderLibrary);
        void copyNativeToRAM(uint8_t *dst, uint32_t dstRowWidth, uint32_t dstRowStart, uint32_t dstRowEnd);
        void clearChanged();
        void addDitherPatterns(const std::array<uint32_t, 4> &extraPatterns);
        uint32_t bestDitherPattern() const;
    };

    struct FramebufferTile {
        uint32_t address;
        uint8_t siz;
        uint8_t fmt;
        uint32_t left;
        uint32_t top;
        uint32_t right;
        uint32_t bottom;
        uint32_t lineWidth;
        uint32_t ditherPattern;

        bool valid() const {
            return (bottom > top) && (right > left);
        }
    };
};