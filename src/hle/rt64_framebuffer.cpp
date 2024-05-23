//
// RT64
//

#include "rt64_framebuffer.h"

#include <algorithm>
#include <cassert>
#include <memory.h>
#include <stdlib.h>

#include "xxHash/xxh3.h"

#include "common/rt64_common.h"
#include "common/rt64_elapsed_timer.h"

#include "render/rt64_render_worker.h"

#ifdef __GNUC__
#define _byteswap_ulong __builtin_bswap32
#endif

#ifndef NDEBUG
//# define DUMP_RAW_RDRAM
#endif

namespace RT64 {
    // Framebuffer

    Framebuffer::Framebuffer() {
        addressStart = 0;
        addressEnd = 0;
        siz = 0;
        width = 0;
        height = 0;
        maxHeight = 0;
        modifiedBytes = 0;
        RAMBytes = 0;
        RAMHash = 0;
        ditherPatterns.fill(0);
        lastWriteType = Type::None;
        lastWriteFmt = 0;
        lastWriteTimestamp = 0;
        widthChanged = false;
        sizChanged = false;
        rdramChanged = false;
        interpolationEnabled = false;
        everUsedAsDepth = false;
    }

    Framebuffer::~Framebuffer() { }

    uint32_t Framebuffer::imageRowBytes(uint32_t rowWidth) const {
        return rowWidth << siz >> 1;
    }

    bool Framebuffer::contains(uint32_t start, uint32_t end) const {
        return (start >= addressStart) && (end <= addressEnd);
    }

    bool Framebuffer::overlaps(uint32_t start, uint32_t end) const {
        return (addressStart < end) && (addressEnd > start);
    }

    void Framebuffer::discardLastWrite() {
        lastWriteType = Type::None;
        lastWriteRect.reset();
    }

    bool Framebuffer::isLastWriteDifferent(Framebuffer::Type newType) const {
        return (lastWriteType != Type::None) && (lastWriteType != newType);
    }

    uint32_t Framebuffer::copyRAMToNativeAndChanges(RenderWorker *worker, FramebufferChange &fbChange, const uint8_t *src, uint32_t rowStart, uint32_t rowCount, uint8_t fmt, bool invalidateTargets, const ShaderLibrary *shaderLibrary) {
        assert(worker != nullptr);
        assert(src != nullptr);

        // Swap the endianness from the source.
        const uint32_t nativeSize = NativeTarget::getNativeSize(width, rowCount, siz);
        if (nativeSwappedRAM.size() < nativeSize) {
            nativeSwappedRAM.resize(nativeSize);
        }

        const uint32_t *srcWords = reinterpret_cast<const uint32_t *>(src);
        uint32_t *dstWords = reinterpret_cast<uint32_t *>(nativeSwappedRAM.data());
        uint32_t wordsToSwap = nativeSize / sizeof(uint32_t);
        while (wordsToSwap > 0) {
            *dstWords = _byteswap_ulong(*srcWords);
            wordsToSwap--;
            srcWords++;
            dstWords++;
        }

        uint32_t differentPixels = nativeTarget.copyFromRAM(worker, fbChange, width, rowCount, rowStart, siz, fmt, nativeSwappedRAM.data(), invalidateTargets, shaderLibrary);
        return differentPixels;
    }

    FramebufferChange *Framebuffer::readChangeFromBytes(RenderWorker *worker, FramebufferChangePool &fbChangePool, Type type, uint8_t fmt, const uint8_t *src, uint32_t rowStart, uint32_t rowCount, const ShaderLibrary *shaderLibrary) {
        assert(worker != nullptr);
        assert(src != nullptr);
        
        FramebufferChange &changeUsed = fbChangePool.use(worker, (type == Type::Depth) ? FramebufferChange::Type::Depth : FramebufferChange::Type::Color, width, rowCount, shaderLibrary->usesHDR);
        uint32_t readPixels = copyRAMToNativeAndChanges(worker, changeUsed, src, rowStart, rowCount, fmt, true, shaderLibrary);
        if (readPixels > 0) {
            return &changeUsed;
        }
        else {
            return nullptr;
        }
    }

    FramebufferChange *Framebuffer::readChangeFromStorage(RenderWorker *worker, const FramebufferStorage &fbStorage, FramebufferChangePool &fbChangePool,
        Type type, uint8_t fmt, uint32_t maxFbPairIndex, uint32_t rowStart, uint32_t rowCount, const ShaderLibrary *shaderLibrary)
    {
        assert(worker != nullptr);

        uint32_t rowBytes = imageRowBytes(width);
        const FramebufferStorage::Handle *storageHandle = fbStorage.get(maxFbPairIndex, addressStart + rowBytes * rowStart);
        if (storageHandle != nullptr) {
            const uint8_t *RDRAM = fbStorage.getRDRAM(*storageHandle);
            return readChangeFromBytes(worker, fbChangePool, type, fmt, RDRAM, rowStart, rowCount, shaderLibrary);
        }
        else {
            return nullptr;
        }
    }

    void Framebuffer::copyRenderTargetToNative(RenderWorker *worker, RenderTarget *target, uint32_t dstRowWidth, uint32_t dstRowStart, uint32_t dstRowEnd, uint8_t fmt, uint32_t ditherRandomSeed, const ShaderLibrary *shaderLibrary) {
        assert(worker != nullptr);
        assert(target != nullptr);
        assert(dstRowStart < height);
        assert(dstRowEnd <= height);

        nativeTarget.copyToNative(worker, target, dstRowWidth, dstRowStart, dstRowEnd, siz, fmt, bestDitherPattern(), ditherRandomSeed, shaderLibrary);
    }

    void Framebuffer::copyNativeToRAM(uint8_t *dst, uint32_t dstRowWidth, uint32_t dstRowStart, uint32_t dstRowEnd) {
        assert(dst != nullptr);
        assert(dstRowStart < height);
        assert(dstRowEnd <= height);

        // Copy native target to RDRAM.
        uint8_t *dstBytes = dst + dstRowStart * imageRowBytes(dstRowWidth);
        uint32_t *dstWords = reinterpret_cast<uint32_t *>(dstBytes);
        uint32_t bytesToSwap = (dstRowEnd - dstRowStart) * imageRowBytes(dstRowWidth);
        uint32_t dstFirstWord = dstWords[0];
        nativeTarget.copyToRAM(dstRowStart, dstRowEnd, dstRowWidth, siz, dstBytes);

        // Write back to RDRAM by swapping every word.
        if (bytesToSwap >= sizeof(uint32_t)) {
            uint32_t wordsToSwap = (bytesToSwap) / sizeof(uint32_t);
            while (wordsToSwap > 0) {
                *dstWords = _byteswap_ulong(*dstWords);
                wordsToSwap--;
                dstWords++;
            }
        }
        // Special case when the total amount of bytes is smaller than a word.
        else {
            uint8_t *dstFirstWordU8 = reinterpret_cast<uint8_t *>(&dstFirstWord);
            for (uint32_t i = 0; i < bytesToSwap; i++) {
                dstFirstWordU8[i ^ 3] = dstBytes[i];
            }

            dstWords[0] = dstFirstWord;
        }

#   ifdef DUMP_RAW_RDRAM
        char path[1024];
        snprintf(path, sizeof(path), "dumps/FB_0x%X_W_%d_H_%d_SIZ_%d.rdram", addressStart, dstRowWidth, height, siz);
        FILE *fp = fopen(path, "wb");
        fwrite(dst, imageRowBytes(dstRowWidth) * dstRowEnd, 1, fp);
        fclose(fp);
#   endif
    }

    void Framebuffer::clearChanged() {
        widthChanged = false;
        sizChanged = false;
        rdramChanged = false;
    }

    void Framebuffer::addDitherPatterns(const std::array<uint32_t, 4> &extraPatterns) {
        for (uint32_t i = 0; i < ditherPatterns.size(); i++) {
            ditherPatterns[i] += extraPatterns[i];
        }
    }
    
    uint32_t Framebuffer::bestDitherPattern() const {
        return std::max_element(ditherPatterns.begin(), ditherPatterns.end()) - ditherPatterns.begin();
    }
};