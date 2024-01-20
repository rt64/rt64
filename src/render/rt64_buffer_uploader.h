//
// RT64
//

#pragma once

#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

#include "rt64_render_worker.h"

namespace RT64 {
    struct BufferPair {
        std::unique_ptr<RenderBuffer> uploadBuffer;
        std::unique_ptr<RenderBuffer> defaultBuffer;
        std::vector<std::unique_ptr<RenderBufferFormattedView>> defaultViews;
        uint64_t allocatedSize = 0;

        const RenderBuffer *get() const {
            return defaultBuffer.get();
        }

        const RenderBufferFormattedView *getView(uint32_t index) const {
            if (index < uint32_t(defaultViews.size())) {
                return defaultViews[index].get();
            }
            else {
                return nullptr;
            }
        }
    };

    struct BufferUploader {
        struct Upload {
            const void *srcData;
            std::pair<size_t, size_t> srcDataIndexRange;
            size_t srcDataStride;
            RenderBufferFlags bufferFlags;
            std::vector<RenderFormat> formatViews;
            BufferPair *dstPair;

            bool valid() const;
        };

        std::thread *thread;
        std::atomic<bool> running;
        bool workAvailable;
        std::mutex workMutex;
        std::mutex readyMutex;
        std::condition_variable workCondition;
        std::condition_variable readyCondition;
        RenderDevice *device;
        std::vector<Upload> pendingUploads;

        BufferUploader(RenderDevice *device);
        ~BufferUploader();
        void threadLoop();
        void threadUpload(const Upload &upload);
        void updateResources(RenderWorker *worker, std::vector<Upload> &blankUploads); // Upload data does not need to be filled in with valid data, only the sizes.
        void commandListBeforeBarriers(RenderWorker *worker);
        void commandListCopyResources(RenderWorker *worker);
        void commandListAfterBarriers(RenderWorker *worker);
        void submit(RenderWorker *worker, const std::vector<Upload> &uploads);
        void wait();
    };
};