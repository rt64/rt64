//
// RT64
//

#include "xxHash/xxh3.h"

#include "common/rt64_thread.h"
#include "hle/rt64_workload_queue.h"

#include "rt64_texture_cache.h"

namespace RT64 {
    // TextureMap

    TextureMap::TextureMap() {
        globalVersion = 0;
        lockCounter = 0;
    }

    TextureMap::~TextureMap() {
        for (const Texture *texture : textures) {
            delete texture;
        }
    }

    void TextureMap::add(uint64_t hash, uint64_t creationFrame, const Texture *texture) {
        assert(hashMap.find(hash) == hashMap.end());

        // Check for free spaces on the LIFO queue first.
        uint32_t textureIndex;
        if (!freeSpaces.empty()) {
            textureIndex = freeSpaces.back();
            freeSpaces.pop_back();
        }
        else {
            textureIndex = static_cast<uint32_t>(textures.size());
            textures.push_back(nullptr);
            hashes.push_back(0);
            versions.push_back(0);
            creationFrames.push_back(0);
            listIterators.push_back(accessList.end());
        }

        hashMap[hash] = textureIndex;
        textures[textureIndex] = texture;
        hashes[textureIndex] = hash;
        versions[textureIndex]++;
        creationFrames[textureIndex] = creationFrame;
        globalVersion++;

        accessList.push_front({ textureIndex, creationFrame });
        listIterators[textureIndex] = accessList.begin();
    }

    bool TextureMap::use(uint64_t hash, uint64_t submissionFrame, uint32_t &textureIndex) {
        // Find the matching texture index in the hash map.
        const auto it = hashMap.find(hash);
        if (it == hashMap.end()) {
            textureIndex = 0;
            return false;
        }

        textureIndex = it->second;

        // Remove the existing entry from the list if it exists.
        AccessList::iterator listIt = listIterators[textureIndex];
        if (listIt != accessList.end()) {
            accessList.erase(listIt);
        }

        // Push a new access entry to the front of the list and store the new iterator.
        accessList.push_front({ textureIndex, submissionFrame });
        listIterators[textureIndex] = accessList.begin();
        return true;
    }

    bool TextureMap::evict(uint64_t submissionFrame, std::vector<uint64_t> &evictedHashes) {
        evictedHashes.clear();

        auto it = accessList.rbegin();
        while (it != accessList.rend()) {
            assert(submissionFrame >= it->second);

            // The max age allowed is the difference between the last time the texture was used and the time it was uploaded.
            // Ensure the textures live long enough for the frame queue to use them.
            const uint64_t MinimumMaxAge = WORKLOAD_QUEUE_SIZE * 2;
            const uint64_t age = submissionFrame - it->second;
            const uint64_t maxAge = std::max(it->second - creationFrames[it->first], MinimumMaxAge);

            // Evict all entries that are present in the access list and are older than the frame by the specified margin.
            if (age >= maxAge) {
                const uint32_t textureIndex = it->first;
                const uint64_t textureHash = hashes[textureIndex];
                evictedTextures.emplace_back(textures[textureIndex]);
                textures[textureIndex] = nullptr;
                hashes[textureIndex] = 0;
                creationFrames[textureIndex] = 0;
                freeSpaces.push_back(textureIndex);
                listIterators[textureIndex] = accessList.end();
                hashMap.erase(textureHash);
                evictedHashes.push_back(textureHash);
                it = decltype(it)(accessList.erase(std::next(it).base()));
            }
            // Stop iterating if we reach an entry that has been used in the present.
            else if (age == 0) {
                break;
            }
            else {
                it++;
            }
        }

        return !evictedHashes.empty();
    }

    void TextureMap::incrementLock() {
        lockCounter++;
    }

    void TextureMap::decrementLock() {
        assert(lockCounter > 0);
        lockCounter--;

        if ((lockCounter == 0) && !evictedTextures.empty()) {
            for (const Texture *texture : evictedTextures) {
                delete texture;
            }

            evictedTextures.clear();
        }
    }

    const Texture *TextureMap::get(uint32_t index) const {
        assert(index < textures.size());
        return textures[index];
    }

    size_t TextureMap::getMaxIndex() const {
        return textures.size();
    }

    // TextureCache

    TextureCache::TextureCache(RenderWorker *worker, const ShaderLibrary *shaderLibrary, bool developerMode) {
        assert(worker != nullptr);

        this->worker = worker;
        this->shaderLibrary = shaderLibrary;
        this->developerMode = developerMode;

        uploadThread = nullptr;
        uploadThreadRunning = false;

        uploadThread = new std::thread(&TextureCache::uploadThreadLoop, this);

        RenderPoolDesc poolDesc;
        poolDesc.heapType = RenderHeapType::UPLOAD;
        poolDesc.useLinearAlgorithm = true;
        poolDesc.allowOnlyBuffers = true;
        uploadResourcePool = worker->device->createPool(poolDesc);
    }

    TextureCache::~TextureCache() {
        if (uploadThread != nullptr) {
            uploadThreadRunning = false;
            uploadQueueChanged.notify_all();
            uploadThread->join();
            delete uploadThread;
        }
        
        descriptorSets.clear();
        uploadResources.clear();
        uploadResourcePool.reset(nullptr);
    }
    
    void TextureCache::setRGBA32(Texture *dstTexture, RenderWorker *worker, const void *bytes, int byteCount, int width, int height, int rowPitch, std::unique_ptr<RenderBuffer> &dstUploadResource, RenderPool *uploadResourcePool) {
        assert(dstTexture != nullptr);
        assert(worker != nullptr);
        assert(bytes != nullptr);
        assert(width > 0);
        assert(height > 0);

        dstTexture->format = RenderFormat::R8G8B8A8_UNORM;
        dstTexture->width = width;
        dstTexture->height = height;

        // Calculate the minimum row width required to store the texture.
        uint32_t rowByteWidth, rowBytePadding;
        CalculateTextureRowWidthPadding(rowPitch, rowByteWidth, rowBytePadding);

        dstTexture->texture = worker->device->createTexture(RenderTextureDesc::Texture2D(width, height, 1, dstTexture->format));
        dstUploadResource = uploadResourcePool->createBuffer(RenderBufferDesc::UploadBuffer(rowByteWidth * height));
        uint8_t *dstData = reinterpret_cast<uint8_t *>(dstUploadResource->map());
        if (rowBytePadding == 0) {
            memcpy(dstData, bytes, byteCount);
        }
        else {
            const uint8_t *srcData = reinterpret_cast<const uint8_t *>(bytes);
            size_t offset = 0;
            while ((offset + rowPitch) <= byteCount) {
                memcpy(dstData, srcData, rowPitch);
                srcData += rowPitch;
                offset += rowPitch;
                dstData += rowByteWidth;
            }
        }

        dstUploadResource->unmap();

        uint32_t rowWidth = rowByteWidth / RenderFormatSize(dstTexture->format);
        worker->commandList->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(dstTexture->texture.get(), RenderTextureLayout::COPY_DEST));
        worker->commandList->copyTextureRegion(RenderTextureCopyLocation::Subresource(dstTexture->texture.get()), RenderTextureCopyLocation::PlacedFootprint(dstUploadResource.get(), dstTexture->format, width, height, 1, rowWidth));
        worker->commandList->barriers(RenderBarrierStage::COMPUTE, RenderTextureBarrier(dstTexture->texture.get(), RenderTextureLayout::SHADER_READ));
    }

    void TextureCache::uploadThreadLoop() {
        Thread::setCurrentThreadName("RT64 Texture");

        uploadThreadRunning = true;

        std::vector<TextureUpload> queueCopy;
        std::vector<TextureUpload> newQueue;
        std::vector<Texture *> texturesUploaded;
        std::vector<RenderTextureBarrier> beforeCopyBarriers;
        std::vector<RenderTextureBarrier> beforeDecodeBarriers;
        std::vector<RenderTextureBarrier> afterDecodeBarriers;

        while (uploadThreadRunning) {
            // Check the top of the queue or wait if it's empty.
            {
                std::unique_lock<std::mutex> queueLock(uploadQueueMutex);
                uploadQueueChanged.wait(queueLock, [this]() {
                    return !uploadThreadRunning || !uploadQueue.empty();
                });

                if (!uploadQueue.empty()) {
                    queueCopy = uploadQueue;
                }
            }

            if (!queueCopy.empty()) {
                // Create new upload buffers and descriptor heaps to fill out the required size.
                const size_t queueSize = queueCopy.size();
                const uint64_t TMEMSize = 0x1000;
                for (size_t i = uploadResources.size(); i < queueSize; i++) {
                    uploadResources.emplace_back(uploadResourcePool->createBuffer(RenderBufferDesc::UploadBuffer(TMEMSize)));
                }

                for (size_t i = descriptorSets.size(); i < queueSize; i++) {
                    descriptorSets.emplace_back(std::make_unique<TextureDecodeDescriptorSet>(worker->device));
                }

                // Upload all textures in the queue.
                {
                    RenderWorkerExecution execution(worker);
                    texturesUploaded.clear();
                    beforeCopyBarriers.clear();
                    for (size_t i = 0; i < queueSize; i++) {
                        static uint32_t TMEMGlobalCounter = 0;
                        const TextureUpload &upload = queueCopy[i];
                        Texture *newTexture = new Texture();
                        newTexture->hash = upload.hash;
                        newTexture->creationFrame = upload.creationFrame;
                        texturesUploaded.emplace_back(newTexture);

                        if (developerMode) {
                            newTexture->bytesTMEM = upload.bytesTMEM;
                        }

                        newTexture->format = RenderFormat::R8_UINT;
                        newTexture->width = int(upload.bytesTMEM.size());
                        newTexture->height = 1;
                        newTexture->tmem = worker->device->createTexture(RenderTextureDesc::Texture1D(newTexture->width, newTexture->height, newTexture->format));
                        newTexture->tmem->setName("Texture Cache TMEM #" + std::to_string(TMEMGlobalCounter++));

                        void *dstData = uploadResources[i]->map();
                        memcpy(dstData, upload.bytesTMEM.data(), upload.bytesTMEM.size());
                        uploadResources[i]->unmap();

                        beforeCopyBarriers.emplace_back(RenderTextureBarrier(newTexture->tmem.get(), RenderTextureLayout::COPY_DEST));
                    }

                    worker->commandList->barriers(RenderBarrierStage::COPY, beforeCopyBarriers);

                    beforeDecodeBarriers.clear();
                    for (size_t i = 0; i < queueSize; i++) {
                        const TextureUpload &upload = queueCopy[i];
                        const uint32_t byteCount = uint32_t(upload.bytesTMEM.size());
                        Texture *dstTexture = texturesUploaded[i];
                        worker->commandList->copyTextureRegion(
                            RenderTextureCopyLocation::Subresource(texturesUploaded[i]->tmem.get()), 
                            RenderTextureCopyLocation::PlacedFootprint(uploadResources[i].get(), RenderFormat::R8_UINT, byteCount, 1, 1, byteCount)
                        );

                        beforeDecodeBarriers.emplace_back(RenderTextureBarrier(dstTexture->tmem.get(), RenderTextureLayout::SHADER_READ));

                        if ((upload.width > 0) && (upload.height > 0)) {
                            static uint32_t TextureGlobalCounter = 0;
                            TextureDecodeDescriptorSet *descSet = descriptorSets[i].get();
                            dstTexture->format = RenderFormat::R8G8B8A8_UNORM;
                            dstTexture->width = upload.width;
                            dstTexture->height = upload.height;
                            dstTexture->texture = worker->device->createTexture(RenderTextureDesc::Texture2D(upload.width, upload.height, 1, dstTexture->format, RenderTextureFlag::STORAGE | RenderTextureFlag::UNORDERED_ACCESS));
                            dstTexture->texture->setName("Texture Cache RGBA32 #" + std::to_string(TextureGlobalCounter++));
                            descSet->setTexture(descSet->TMEM, dstTexture->tmem.get(), RenderTextureLayout::SHADER_READ);
                            descSet->setTexture(descSet->RGBA32, dstTexture->texture.get(), RenderTextureLayout::GENERAL);
                            beforeDecodeBarriers.emplace_back(RenderTextureBarrier(dstTexture->texture.get(), RenderTextureLayout::GENERAL));
                        }
                    }
                    
                    worker->commandList->barriers(RenderBarrierStage::COMPUTE, beforeDecodeBarriers);

                    const ShaderRecord &textureDecode = shaderLibrary->textureDecode;
                    bool pipelineSet = false;
                    afterDecodeBarriers.clear();
                    for (size_t i = 0; i < queueSize; i++) {
                        const TextureUpload &upload = queueCopy[i];
                        if ((upload.width > 0) && (upload.height > 0)) {
                            if (!pipelineSet) {
                                worker->commandList->setPipeline(textureDecode.pipeline.get());
                                worker->commandList->setComputePipelineLayout(textureDecode.pipelineLayout.get());
                            }

                            interop::TextureDecodeCB decodeCB;
                            decodeCB.Resolution.x = upload.width;
                            decodeCB.Resolution.y = upload.height;
                            decodeCB.fmt = upload.loadTile.fmt;
                            decodeCB.siz = upload.loadTile.siz;
                            decodeCB.address = interop::uint(upload.loadTile.tmem) << 3;
                            decodeCB.stride = interop::uint(upload.loadTile.line) << 3;
                            decodeCB.tlut = upload.tlut;
                            decodeCB.palette = upload.loadTile.palette;

                            // Dispatch compute shader for decoding texture.
                            const uint32_t ThreadGroupSize = 8;
                            const uint32_t dispatchX = (decodeCB.Resolution.x + ThreadGroupSize - 1) / ThreadGroupSize;
                            const uint32_t dispatchY = (decodeCB.Resolution.y + ThreadGroupSize - 1) / ThreadGroupSize;
                            worker->commandList->setComputePushConstants(0, &decodeCB);
                            worker->commandList->setComputeDescriptorSet(descriptorSets[i]->get(), 0);
                            worker->commandList->dispatch(dispatchX, dispatchY, 1);

                            afterDecodeBarriers.emplace_back(RenderTextureBarrier(texturesUploaded[i]->texture.get(), RenderTextureLayout::SHADER_READ));
                        }
                    }

                    if (!afterDecodeBarriers.empty()) {
                        worker->commandList->barriers(RenderBarrierStage::COMPUTE, afterDecodeBarriers);
                    }
                }

                // Add all the textures to the map once they're ready.
                {
                    const std::unique_lock<std::mutex> lock(textureMapMutex);
                    for (Texture *texture : texturesUploaded) {
                        textureMap.add(texture->hash, texture->creationFrame, texture);
                    }
                }

                // Make the new queue the remaining subsection of the upload queue that wasn't processed in this batch.
                {
                    const std::unique_lock<std::mutex> queueLock(uploadQueueMutex);
                    newQueue = std::vector<TextureUpload>(uploadQueue.begin() + queueSize, uploadQueue.end());
                    uploadQueue = std::move(newQueue);
                }

                queueCopy.clear();
                uploadQueueFinished.notify_all();
            }
        }
    }

    void TextureCache::queueGPUUploadTMEM(uint64_t hash, uint64_t creationFrame, const uint8_t *bytes, int bytesCount, int width, int height, uint32_t tlut, const LoadTile &loadTile) {
        assert(bytes != nullptr);
        assert(bytesCount > 0);

        TextureUpload newUpload;
        newUpload.hash = hash;
        newUpload.creationFrame = creationFrame;
        newUpload.width = width;
        newUpload.height = height;
        newUpload.tlut = tlut;
        newUpload.loadTile = loadTile;
        newUpload.bytesTMEM = std::vector<uint8_t>(bytes, bytes + bytesCount);

        {
            const std::unique_lock<std::mutex> queueLock(uploadQueueMutex);
            uploadQueue.emplace_back(newUpload);
        }

        uploadQueueChanged.notify_all();
    }

    void TextureCache::waitForGPUUploads() {
        std::unique_lock<std::mutex> queueLock(uploadQueueMutex);
        uploadQueueFinished.wait(queueLock, [this]() {
            return uploadQueue.empty();
        });
    }

    bool TextureCache::useTexture(uint64_t hash, uint64_t submissionFrame, uint32_t &textureIndex) {
        const std::unique_lock<std::mutex> lock(textureMapMutex);
        return textureMap.use(hash, submissionFrame, textureIndex);
    }

    const Texture *TextureCache::getTexture(uint32_t textureIndex) {
        const std::unique_lock<std::mutex> lock(textureMapMutex);
        return textureMap.get(textureIndex);
    }

    bool TextureCache::evict(uint64_t submissionFrame, std::vector<uint64_t> &evictedHashes) {
        const std::unique_lock<std::mutex> lock(textureMapMutex);
        return textureMap.evict(submissionFrame, evictedHashes);
    }

    void TextureCache::incrementLock() {
        const std::unique_lock<std::mutex> lock(textureMapMutex);
        textureMap.incrementLock();
    }

    void TextureCache::decrementLock() {
        const std::unique_lock<std::mutex> lock(textureMapMutex);
        textureMap.decrementLock();
    }
};