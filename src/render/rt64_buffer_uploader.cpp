//
// RT64
//

#include <algorithm>
#include <cstring>

#include "common/rt64_thread.h"

#include "rt64_buffer_uploader.h"

namespace RT64 {
    // Common functions.

    static uint64_t roundUp(uint64_t value, uint64_t powerOf2Alignment) {
        return (value + powerOf2Alignment - 1) & ~(powerOf2Alignment - 1);
    }

    // BufferUploader::Upload

    bool BufferUploader::Upload::valid() const {
        return (srcData != nullptr) && (srcDataIndexRange.second > srcDataIndexRange.first);
    }

    // BufferUploader

    BufferUploader::BufferUploader(RenderDevice *device) {
        assert(device != nullptr);

        this->device = device;
        workAvailable = false;
        thread = new std::thread(&BufferUploader::threadLoop, this);
    }

    BufferUploader::~BufferUploader() {
        running = false;
        workCondition.notify_all();
        thread->join();
        delete thread;
    }

    void BufferUploader::threadLoop() {
        Thread::setCurrentThreadName("RT64 Buffer");

        running = true;

        while (running) {
            std::unique_lock<std::mutex> queueLock(workMutex);
            workCondition.wait(queueLock, [this]() {
                return !running || workAvailable;
            });
            
            if (running) {
                for (const Upload &u : pendingUploads) {
                    threadUpload(u);
                }
            }

            {
                std::unique_lock<std::mutex> readyLock(readyMutex);
                workAvailable = false;
            }

            readyCondition.notify_all();
        }
    }

    void BufferUploader::threadUpload(const Upload &upload) {
        if (!upload.valid()) {
            return;
        }

        assert(upload.dstPair != nullptr);
        const size_t srcOffset = upload.srcDataIndexRange.first * upload.srcDataStride;
        const size_t srcSize = (upload.srcDataIndexRange.second - upload.srcDataIndexRange.first) * upload.srcDataStride;
        const RenderRange writtenRange(srcOffset, srcOffset + srcSize);
        uint8_t *dstData = static_cast<uint8_t *>(upload.dstPair->uploadBuffer->map());
        memcpy(dstData + srcOffset, static_cast<const uint8_t *>(upload.srcData) + srcOffset, srcSize);
        upload.dstPair->uploadBuffer->unmap(0, &writtenRange);
    }

    void BufferUploader::updateResources(RenderWorker *worker, std::vector<Upload> &blankUploads) {
        for (Upload &u : blankUploads) {
            // Ignore the reallocation of the buffer if the required size is already enough. We always create a buffer if it hasn't been created yet.
            const size_t requiredSize = u.srcDataIndexRange.second * u.srcDataStride;
            BufferPair &bufferPair = *u.dstPair;
            if ((bufferPair.defaultBuffer != nullptr) && (!u.valid() || (bufferPair.allocatedSize >= requiredSize))) {
                continue;
            }

            bufferPair.defaultViews.clear();
            
            // Recreate the buffer pair.
            const uint64_t BlockAlignment = 256;
            bufferPair.allocatedSize = std::max(uint64_t((requiredSize * 3) / 2), BlockAlignment);
            bufferPair.allocatedSize = roundUp(bufferPair.allocatedSize, BlockAlignment);
            bufferPair.uploadBuffer = worker->device->createBuffer(RenderBufferDesc::UploadBuffer(bufferPair.allocatedSize));
            bufferPair.defaultBuffer = worker->device->createBuffer(RenderBufferDesc::DefaultBuffer(bufferPair.allocatedSize, u.bufferFlags));

            bufferPair.defaultViews.reserve(u.formatViews.size());
            for (RenderFormat format : u.formatViews) {
                bufferPair.defaultViews.emplace_back(bufferPair.defaultBuffer->createBufferFormattedView(format));
            }

            // Since the buffers had to be recreated, reupload all the data by modifying the source upload.
            u.srcDataIndexRange.first = 0;
        }
    }

    void BufferUploader::submit(RenderWorker *worker, const std::vector<Upload> &uploads) {
        {
            std::unique_lock<std::mutex> queueLock(workMutex);
            pendingUploads = uploads;
            updateResources(worker, pendingUploads);
            workAvailable = true;
        }

        workCondition.notify_all();
    }

    void BufferUploader::commandListBeforeBarriers(RenderWorker *worker) {
        thread_local std::vector<RenderBufferBarrier> beforeBarriers;
        beforeBarriers.clear();

        for (const Upload &u : pendingUploads) {
            if (!u.valid()) {
                continue;
            }

            auto &defaultBuffer = u.dstPair->defaultBuffer;
            beforeBarriers.push_back(RenderBufferBarrier(defaultBuffer.get(), RenderBufferAccess::WRITE));
        }

        if (!beforeBarriers.empty()) {
            worker->commandList->barriers(RenderBarrierStage::COPY, beforeBarriers);
        }
    }

    void BufferUploader::commandListCopyResources(RenderWorker *worker) {
        for (const Upload &u : pendingUploads) {
            if (!u.valid()) {
                continue;
            }

            const uint64_t srcOffset = u.srcDataIndexRange.first * u.srcDataStride;
            const uint64_t srcSize = (u.srcDataIndexRange.second - u.srcDataIndexRange.first) * u.srcDataStride;
            worker->commandList->copyBufferRegion(u.dstPair->defaultBuffer->at(srcOffset), u.dstPair->uploadBuffer->at(srcOffset), srcSize);
        }
    }

    void BufferUploader::commandListAfterBarriers(RenderWorker *worker) {
        thread_local std::vector<RenderBufferBarrier> afterBarriers;
        afterBarriers.clear();

        for (const Upload &u : pendingUploads) {
            if (!u.valid()) {
                continue;
            }

            auto &defaultBuffer = u.dstPair->defaultBuffer;
            afterBarriers.push_back(RenderBufferBarrier(defaultBuffer.get(), RenderBufferAccess::READ));
        }

        if (!afterBarriers.empty()) {
            worker->commandList->barriers(RenderBarrierStage::ALL, afterBarriers);
        }
    }
    
    void BufferUploader::wait() {
        std::unique_lock<std::mutex> readyLock(readyMutex);
        readyCondition.wait(readyLock, [this]() {
            return !workAvailable;
        });
    }
};