//
// RT64
//

#pragma once

#include <stdint.h>

#include "common/rt64_common.h"
#include "hle/rt64_framebuffer_changes.h"

#include "rt64_descriptor_sets.h"
#include "rt64_shader_library.h"

namespace RT64 {
    struct RenderWorker;
    struct RenderTarget;

    struct NativeTarget {
        struct ReadBuffer {
            std::unique_ptr<RenderBuffer> nativeBuffer;
            std::unique_ptr<RenderBuffer> nativeUploadBuffer;
            std::unique_ptr<RenderBufferFormattedView> nativeBufferView;
            std::unique_ptr<RenderBufferFormattedView> nativeBufferNextView;
            std::unique_ptr<RenderBufferFormattedView> nativeBufferWriteView;
            std::unique_ptr<FramebufferReadChangesDescriptorBufferSet> readDescSet;
            uint32_t nativeBufferSize = 0;
            uint8_t nativeBufferViewFormat = 0;
            uint8_t nativeBufferNextViewFormat = 0;
            uint8_t nativeBufferWriteViewFormat = 0;
        };

        struct WriteBuffer {
            std::unique_ptr<FramebufferWriteDescriptorBufferSet> writeDescSet;
            std::unique_ptr<RenderBuffer> nativeReadbackBuffer;
            uint32_t nativeReadbackBufferSize = 0;
        };

        std::unique_ptr<RenderBuffer> changeCountBuffer;
        std::unique_ptr<RenderBuffer> changeReadbackBuffer;
        std::unique_ptr<FramebufferClearChangesDescriptorSet> clearDescSet;
        std::vector<ReadBuffer> readBufferHistory;
        std::vector<WriteBuffer> writeBufferHistory;
        uint32_t readBufferHistoryCount = 0;
        uint32_t writeBufferHistoryCount = 0;
        uint32_t writeBufferHistoryIndex = 0;

        NativeTarget();
        ~NativeTarget();

        void resetBufferHistory();
        void createReadBuffer(RenderWorker *worker, ReadBuffer &readBuffer, uint32_t bufferSize);
        RenderFormat getBufferFormat(uint8_t siz) const;

        // Returns the amount of different pixels.
        uint32_t copyFromRAM(RenderWorker *worker, FramebufferChange &emptyFbChange, uint32_t width, uint32_t height, uint32_t rowStart, uint8_t siz, uint8_t fmt, const uint8_t *data, bool invalidateTargets, const ShaderLibrary *shaderLibrary);
        void copyToNative(RenderWorker *worker, RenderTarget *srcTarget, uint32_t rowWidth, uint32_t rowStart, uint32_t rowEnd, uint8_t siz, uint8_t fmt, uint32_t ditherPattern, uint32_t ditherRandomSeed, const ShaderLibrary *shaderLibrary);
        void copyToRAM(uint32_t rowStart, uint32_t rowEnd, uint32_t width, uint8_t siz, uint8_t *data);

        static uint32_t getNativeSize(uint32_t width, uint32_t height, uint8_t siz);
    };
};