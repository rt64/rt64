//
// RT64
//

#define STB_IMAGE_IMPLEMENTATION

#include "ddspp/ddspp.h"
#include "stb/stb_image.h"
#include "xxHash/xxh3.h"

// Needs to be included before other headers.
#include "gbi/rt64_f3d.h"

#include "common/rt64_filesystem_directory.h"
#include "common/rt64_filesystem_zip.h"
#include "common/rt64_load_types.h"
#include "common/rt64_thread.h"
#include "common/rt64_tmem_hasher.h"
#include "hle/rt64_workload_queue.h"

#include "rt64_texture_cache.h"

#define ONLY_USE_LOW_MIP_CACHE 0

namespace RT64 {
    // ReplacementMap
    
    static const interop::float2 IdentityScale = { 1.0f, 1.0f };
    static const uint32_t TextureDataPitchAlignment = 256;
    static const uint32_t TextureDataPlacementAlignment = 512;

    ReplacementMap::ReplacementMap() {
        // Empty constructor.
    }

    ReplacementMap::~ReplacementMap() {
        for (auto it : loadedTextureMap) {
            delete it.second.texture;
        }

        for (auto it : lowMipCacheTextures) {
            delete it.second.texture;
        }
    }

    void ReplacementMap::clear(std::vector<Texture *> &evictedTextures) {
        for (auto it : loadedTextureMap) {
            evictedTextures.emplace_back(it.second.texture);
        }

        for (auto it : lowMipCacheTextures) {
            evictedTextures.emplace_back(it.second.texture);
        }

        loadedTextureMap.clear();
        loadedTextureReverseMap.clear();
        unusedTextureList.clear();
        lowMipCacheTextures.clear();
        replacementDirectories.clear();
        resolvedHashVersions.clear();
        fileSystemStreamResolvedPaths.clear();
        fileSystemHashVersions.clear();

        usedTexturePoolSize = 0;
        cachedTexturePoolSize = 0;
        fileSystemIsDirectory = false;
    }

    void ReplacementMap::evict(std::vector<Texture *> &evictedTextures) {
        while ((cachedTexturePoolSize > maxTexturePoolSize) && !unusedTextureList.empty()) {
            // Push texture to the eviction list.
            Texture *lastUnusedTexture = unusedTextureList.back();
            evictedTextures.emplace_back(lastUnusedTexture);

            // Erase from the maps and the relative path set.
            auto it = loadedTextureReverseMap.find(lastUnusedTexture);
            fileSystemStreamSets[it->second->second.fileSystemIndex].erase(it->second->second.relativePath);
            loadedTextureMap.erase(it->second);
            loadedTextureReverseMap.erase(it);

            // Take texture off the unused list.
            unusedTextureList.pop_back();
            cachedTexturePoolSize -= lastUnusedTexture->memorySize;
        }
    }

    bool ReplacementMap::saveDatabase(std::ostream &stream) {
        try {
            json jroot = directoryDatabase;
            stream << std::setw(4) << jroot << std::endl;
            return !stream.bad();
        }
        catch (const nlohmann::detail::exception &e) {
            fprintf(stderr, "JSON writing error: %s\n", e.what());
            return false;
        }
    }

    void ReplacementMap::removeUnusedEntriesFromDatabase() {
        std::vector<ReplacementTexture> newTextures;
        for (const ReplacementTexture &texture : directoryDatabase.textures) {
            uint64_t rt64 = ReplacementDatabase::stringToHash(texture.hashes.rt64);
            auto &resolvedPaths = fileSystemResolvedPaths[0];
            auto pathIt = resolvedPaths.find(rt64);

            // Only consider for removal if the entry has no assigned path.
            if (texture.path.empty()) {
                if (pathIt == resolvedPaths.end()) {
                    continue;
                }

                if (pathIt->second.relativePath.empty()) {
                    continue;
                }
            }

            newTextures.emplace_back(texture);
        }

        directoryDatabase.textures = newTextures;
        directoryDatabase.buildHashMaps();
    }
    
    void ReplacementMap::addLoadedTexture(Texture *texture, uint32_t fileSystemIndex, const std::string &relativePath, bool referenceCounted) {
        uint64_t pathHash = hashFromRelativePath(fileSystemIndex, relativePath);
        assert(loadedTextureMap.find(pathHash) == loadedTextureMap.end());
        MapEntry &entry = loadedTextureMap[pathHash];
        entry.fileSystemIndex = fileSystemIndex;
        entry.relativePath = relativePath;
        entry.texture = texture;

        if (referenceCounted) {
            entry.unusedTextureListIterator = unusedTextureList.insert(unusedTextureList.begin(), texture);
            loadedTextureReverseMap[texture] = loadedTextureMap.find(pathHash);
            cachedTexturePoolSize += texture->memorySize;
        }
    }

    Texture *ReplacementMap::getFromRelativePath(uint32_t fileSystemIndex, const std::string &relativePath) const {
        uint64_t pathHash = hashFromRelativePath(fileSystemIndex, relativePath);
        auto it = loadedTextureMap.find(pathHash);
        if (it != loadedTextureMap.end()) {
            return it->second.texture;
        }
        else {
            return nullptr;
        }
    }

    uint64_t ReplacementMap::hashFromRelativePath(uint32_t fileSystemIndex, const std::string &relativePath) const {
        XXH3_state_t xxh3;
        XXH3_64bits_reset(&xxh3);
        XXH3_64bits_update(&xxh3, &fileSystemIndex, sizeof(uint32_t));
        XXH3_64bits_update(&xxh3, relativePath.data(), relativePath.size());
        return XXH3_64bits_digest(&xxh3);
    }

    void ReplacementMap::incrementReference(Texture *texture) {
        auto it = loadedTextureReverseMap.find(texture);
        assert(it != loadedTextureReverseMap.end());

        // Remove entry from unused list if reference count was zero.
        MapEntry &entry = it->second->second;
        if (entry.references == 0) {
            assert(entry.unusedTextureListIterator != unusedTextureList.end());
            unusedTextureList.erase(entry.unusedTextureListIterator);
            entry.unusedTextureListIterator = unusedTextureList.end();
            usedTexturePoolSize += texture->memorySize;
        }

        entry.references++;
    }

    void ReplacementMap::decrementReference(Texture *texture) {
        auto it = loadedTextureReverseMap.find(texture);
        MapEntry &entry = it->second->second;
        assert(entry.references > 0);
        entry.references--;

        // Push to unused list if reference count is zero.
        if (entry.references == 0) {
            assert(entry.unusedTextureListIterator == unusedTextureList.end());
            entry.unusedTextureListIterator = unusedTextureList.insert(unusedTextureList.begin(), texture);
            usedTexturePoolSize -= texture->memorySize;
        }
    }

    // TextureMap

    TextureMap::TextureMap() {
        globalVersion = 0;
        replacementMapEnabled = true;
    }

    TextureMap::~TextureMap() {
        for (Texture *texture : textures) {
            delete texture;
        }

        for (Texture *texture : evictedTextures) {
            delete texture;
        }
    }

    void TextureMap::clearReplacements() {
        for (size_t i = 0; i < textureReplacements.size(); i++) {
            if (textureReplacements[i] != nullptr) {
                textureReplacements[i] = nullptr;
                textureReplacementShiftedByHalf[i] = false;
                textureReplacementReferenceCounted[i] = false;
                versions[i]++;
            }
        }

        globalVersion++;
    }

    void TextureMap::add(uint64_t hash, uint64_t creationFrame, Texture *texture) {
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
            cachedTextureDimensions.emplace_back();
            textureReplacements.push_back(nullptr);
            cachedTextureReplacementDimensions.emplace_back();
            textureReplacementShiftedByHalf.emplace_back(false);
            textureReplacementReferenceCounted.emplace_back(false);
            textureScales.push_back(IdentityScale);
            hashes.push_back(0);
            versions.push_back(0);
            creationFrames.push_back(0);
            listIterators.push_back(accessList.end());
        }

        hashMap[hash] = textureIndex;
        textures[textureIndex] = texture;
        cachedTextureDimensions[textureIndex] = interop::float3(float(texture->width), float(texture->height), 1.0f);
        textureReplacements[textureIndex] = nullptr;
        textureReplacementShiftedByHalf[textureIndex] = false;
        textureReplacementReferenceCounted[textureIndex] = false;
        textureScales[textureIndex] = IdentityScale;
        hashes[textureIndex] = hash;
        versions[textureIndex]++;
        creationFrames[textureIndex] = creationFrame;
        globalVersion++;

        accessList.push_front({ textureIndex, creationFrame });
        listIterators[textureIndex] = accessList.begin();
    }

    void TextureMap::replace(uint64_t hash, Texture *texture, bool shiftedByHalf, bool referenceCounted) {
        const auto it = hashMap.find(hash);
        if (it == hashMap.end()) {
            return;
        }

        // Do nothing if it's the same texture. If the operations were done correctly, it's not possible for either the texture or the
        // replacement to be any different, so the operation can be considered a no-op.
        if (texture == textureReplacements[it->second]) {
            // The exception is that the shifting mode can be changed in real-time by the user while editing, so we update it regardless.
            textureReplacementShiftedByHalf[it->second] = shiftedByHalf;
            return;
        }

        // If it's a different texture and a texture was already replaced, decrement its reference.
        if ((textureReplacements[it->second] != nullptr) && textureReplacementReferenceCounted[it->second]) {
            replacementMap.decrementReference(textureReplacements[it->second]);
        }

        Texture *replacedTexture = textures[it->second];
        textureReplacements[it->second] = texture;
        textureReplacementShiftedByHalf[it->second] = shiftedByHalf;
        textureReplacementReferenceCounted[it->second] = referenceCounted;
        textureScales[it->second] = { float(texture->width) / float(replacedTexture->width), float(texture->height) / float(replacedTexture->height) };
        cachedTextureReplacementDimensions[it->second] = interop::float3(float(texture->width), float(texture->height), float(texture->mipmaps));
        versions[it->second]++;
        globalVersion++;

        if (referenceCounted) {
            // Increment reference counter for this replacement so it doesn't get unloaded from the cache.
            replacementMap.incrementReference(texture);
        }
    }

    bool TextureMap::use(uint64_t hash, uint64_t submissionFrame, uint32_t &textureIndex, interop::float2 &textureScale, interop::float3 &textureDimensions, bool &textureReplaced, bool &hasMipmaps, bool &shiftedByHalf) {
        textureScale = IdentityScale;
        hasMipmaps = false;
        shiftedByHalf = false;

        // Find the matching texture index in the hash map.
        const auto it = hashMap.find(hash);
        if (it == hashMap.end()) {
            textureIndex = 0;
            return false;
        }

        textureIndex = it->second;
        textureReplaced = replacementMapEnabled && (textureReplacements[textureIndex] != nullptr);

        if (textureReplaced) {
            textureScale = textureScales[textureIndex];
            textureDimensions = cachedTextureReplacementDimensions[textureIndex];
            hasMipmaps = (textureReplacements[textureIndex]->mipmaps > 1);
            shiftedByHalf = textureReplacementShiftedByHalf[textureIndex];
        }
        else {
            textureDimensions = cachedTextureDimensions[textureIndex];
        }

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
            const uint64_t MaximumMaxAge = WORKLOAD_QUEUE_SIZE * 32;
            const uint64_t age = submissionFrame - it->second;
            const uint64_t maxAge = std::clamp(it->second - creationFrames[it->first], MinimumMaxAge, MaximumMaxAge);

            // Evict all entries that are present in the access list and are older than the frame by the specified margin.
            if (age >= maxAge) {
                const uint32_t textureIndex = it->first;
                const uint64_t textureHash = hashes[textureIndex];
                evictedTextures.emplace_back(textures[textureIndex]);
                textures[textureIndex] = nullptr;
                textureScales[textureIndex] = { 1.0f, 1.0f };
                hashes[textureIndex] = 0;
                creationFrames[textureIndex] = 0;
                freeSpaces.push_back(textureIndex);
                listIterators[textureIndex] = accessList.end();
                hashMap.erase(textureHash);
                evictedHashes.push_back(textureHash);
                it = decltype(it)(accessList.erase(std::next(it).base()));

                // If a texture replacement was used for this texture, decrease the reference in the map.
                if (textureReplacementReferenceCounted[textureIndex] && (textureReplacements[textureIndex] != nullptr)) {
                    replacementMap.decrementReference(textureReplacements[textureIndex]);
                }

                textureReplacements[textureIndex] = nullptr;
                textureReplacementShiftedByHalf[textureIndex] = false;
                textureReplacementReferenceCounted[textureIndex] = false;
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

    Texture *TextureMap::get(uint32_t index) const {
        assert(index < textures.size());
        return textures[index];
    }

    size_t TextureMap::getMaxIndex() const {
        return textures.size();
    }

    // TextureCache::StreamThread

    TextureCache::StreamThread::StreamThread(TextureCache *textureCache) {
        assert(textureCache != nullptr);

        this->textureCache = textureCache;

        threadRunning = true;
        worker = std::make_unique<RenderWorker>(textureCache->directWorker->device, "RT64 Stream Worker", RenderCommandListType::COPY);
        thread = std::make_unique<std::thread>(&StreamThread::loop, this);
    }

    TextureCache::StreamThread::~StreamThread() {
        threadRunning = false;
        textureCache->streamDescStackChanged.notify_all();
        thread->join();
        thread.reset(nullptr);
    }

    void TextureCache::StreamThread::loop() {
        Thread::setCurrentThreadName("RT64 Stream");

        // Texture streaming threads should have a priority somewhere inbetween the main threads and the shader compilation threads.
        Thread::setCurrentThreadPriority(Thread::Priority::Low);

        std::vector<uint8_t> replacementBytes;
        while (threadRunning) {
            StreamDescription streamDesc;

            // Check the top of the queue or wait if it's empty.
            {
                std::unique_lock queueLock(textureCache->streamDescStackMutex);
                textureCache->streamDescStackActiveCount--;
                textureCache->streamDescStackChanged.wait(queueLock, [this]() {
                    return !threadRunning || !textureCache->streamDescStack.empty();
                });

                textureCache->streamDescStackActiveCount++;

                if (!textureCache->streamDescStack.empty()) {
                    streamDesc = textureCache->streamDescStack.top();
                    textureCache->streamDescStack.pop();
                }
            }
            
            if (!streamDesc.relativePath.empty()) {
                ElapsedTimer elapsedTimer;
                bool fileLoaded = textureCache->textureMap.replacementMap.fileSystems[streamDesc.fileSystemIndex]->load(streamDesc.relativePath, replacementBytes);
                textureCache->addStreamLoadTime(elapsedTimer.elapsedMicroseconds());

                if (fileLoaded) {
                    worker->commandList->begin();
                    Texture *texture = TextureCache::loadTextureFromBytes(worker->device, worker->commandList.get(), replacementBytes, uploadResource);
                    worker->commandList->end();

                    // Only execute the command list and wait if the texture was loaded successfully.
                    if (texture != nullptr) {
                        worker->execute();
                        worker->wait();
                        textureCache->uploadQueueMutex.lock();
                        textureCache->streamResultQueue.emplace_back(texture, streamDesc.fileSystemIndex, streamDesc.relativePath, streamDesc.fromPreload);
                        textureCache->uploadQueueMutex.unlock();
                        textureCache->uploadQueueChanged.notify_all();
                    }
                }
            }
        }
    }

    // TextureCache

    TextureCache::TextureCache(RenderWorker *directWorker, RenderWorker *copyWorker, uint32_t threadCount, const ShaderLibrary *shaderLibrary) {
        assert(directWorker != nullptr);

        this->directWorker = directWorker;
        this->copyWorker = copyWorker;
        this->shaderLibrary = shaderLibrary;

        lockCounter = 0;

        // Copy the command list and fence used by the methods called from the main thread.
        loaderCommandList = copyWorker->commandQueue->createCommandList();
        loaderCommandFence = copyWorker->device->createCommandFence();

        // Create the semaphore used to synchronize the copy and the direct command queues.
        copyToDirectSemaphore = directWorker->device->createCommandSemaphore();

        // Create upload pool.
        RenderPoolDesc poolDesc;
        poolDesc.heapType = RenderHeapType::UPLOAD;
        poolDesc.useLinearAlgorithm = true;
        poolDesc.allowOnlyBuffers = true;
        uploadResourcePool = directWorker->device->createPool(poolDesc);

        // Create upload thread.
        uploadThread = std::make_unique<std::thread>(&TextureCache::uploadThreadLoop, this);

        // Create streaming threads.
        streamDescStackActiveCount = threadCount;

        for (uint32_t i = 0; i < threadCount; i++) {
            streamThreads.push_back(std::make_unique<StreamThread>(this));
        }
    }

    TextureCache::~TextureCache() {
        waitForAllStreamThreads(true);
        streamThreads.clear();

        if (uploadThread != nullptr) {
            uploadThreadRunning = false;
            uploadQueueChanged.notify_all();
            uploadThread->join();
            uploadThread.reset();
        }
        
        descriptorSets.clear();
        tmemUploadResources.clear();
        replacementUploadResources.clear();
        uploadResourcePool.reset(nullptr);
    }

    static uint32_t nextSizeAlignedTo(uint32_t size, uint32_t alignment) {
        if (size % alignment) {
            return size + (alignment - (size % alignment));
        }
        else {
            return size;
        }
    }

    static void memcpyRows(uint8_t *dst, uint32_t dstRowPitch, const uint8_t *src, uint32_t srcRowPitch, uint32_t rowCount) {
        assert(dstRowPitch >= srcRowPitch);

        if (dstRowPitch > srcRowPitch) {
            for (size_t i = 0; i < rowCount; i++) {
                memcpy(&dst[i * dstRowPitch], &src[i * srcRowPitch], srcRowPitch);
            }
        }
        else {
            memcpy(dst, src, dstRowPitch * rowCount);
        }
    }
    
    void TextureCache::setRGBA32(Texture *dstTexture, RenderDevice *device, RenderCommandList *commandList, const uint8_t *bytes, size_t byteCount, uint32_t width, uint32_t height, uint32_t rowPitch, std::unique_ptr<RenderBuffer> &dstUploadResource, RenderPool *uploadResourcePool, std::mutex *uploadResourcePoolMutex) {
        assert(dstTexture != nullptr);
        assert(device != nullptr);
        assert(commandList != nullptr);
        assert(bytes != nullptr);
        assert(width > 0);
        assert(height > 0);

        dstTexture->format = RenderFormat::R8G8B8A8_UNORM;
        dstTexture->width = width;
        dstTexture->height = height;
        dstTexture->mipmaps = 1;
        dstTexture->texture = device->createTexture(RenderTextureDesc::Texture2D(width, height, 1, dstTexture->format));

        uint32_t alignedRowPitch = nextSizeAlignedTo(rowPitch, TextureDataPitchAlignment);
        if (uploadResourcePool != nullptr) {
            assert(uploadResourcePoolMutex != nullptr);
            std::unique_lock queueLock(*uploadResourcePoolMutex);
            dstUploadResource = uploadResourcePool->createBuffer(RenderBufferDesc::UploadBuffer(alignedRowPitch * height));
        }
        else {
            dstUploadResource = device->createBuffer(RenderBufferDesc::UploadBuffer(alignedRowPitch * height));
        }

        dstTexture->memorySize = alignedRowPitch * height;

        uint8_t *dstData = reinterpret_cast<uint8_t *>(dstUploadResource->map());
        const uint8_t *srcData = reinterpret_cast<const uint8_t *>(bytes);
        memcpyRows(dstData, alignedRowPitch, srcData, rowPitch, height);
        dstUploadResource->unmap();

        uint32_t alignedRowWidth = alignedRowPitch / RenderFormatSize(dstTexture->format);
        commandList->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(dstTexture->texture.get(), RenderTextureLayout::COPY_DEST));
        commandList->copyTextureRegion(RenderTextureCopyLocation::Subresource(dstTexture->texture.get()), RenderTextureCopyLocation::PlacedFootprint(dstUploadResource.get(), dstTexture->format, width, height, 1, alignedRowWidth));
    }

    static RenderTextureDimension toRenderDimension(ddspp::TextureType type) {
        switch (type) {
        case ddspp::Texture1D:
            return RenderTextureDimension::TEXTURE_1D;
        case ddspp::Texture2D:
            return RenderTextureDimension::TEXTURE_2D;
        case ddspp::Texture3D:
            return RenderTextureDimension::TEXTURE_3D;
        default:
            assert(false && "Unknown texture type from DDS.");
            return RenderTextureDimension::UNKNOWN;
        }
    }

    static RenderFormat toRenderFormat(ddspp::DXGIFormat format) {
        switch (format) {
        case ddspp::R32G32B32A32_TYPELESS:
            return RenderFormat::R32G32B32A32_TYPELESS;
        case ddspp::R32G32B32A32_FLOAT:
            return RenderFormat::R32G32B32A32_FLOAT;
        case ddspp::R32G32B32A32_UINT:
            return RenderFormat::R32G32B32A32_UINT;
        case ddspp::R32G32B32A32_SINT:
            return RenderFormat::R32G32B32A32_SINT;
        case ddspp::R32G32B32_TYPELESS:
            return RenderFormat::R32G32B32_TYPELESS;
        case ddspp::R32G32B32_FLOAT:
            return RenderFormat::R32G32B32_FLOAT;
        case ddspp::R32G32B32_UINT:
            return RenderFormat::R32G32B32_UINT;
        case ddspp::R32G32B32_SINT:
            return RenderFormat::R32G32B32_SINT;
        case ddspp::R16G16B16A16_TYPELESS:
            return RenderFormat::R16G16B16A16_TYPELESS;
        case ddspp::R16G16B16A16_FLOAT:
            return RenderFormat::R16G16B16A16_FLOAT;
        case ddspp::R16G16B16A16_UNORM:
            return RenderFormat::R16G16B16A16_UNORM;
        case ddspp::R16G16B16A16_UINT:
            return RenderFormat::R16G16B16A16_UINT;
        case ddspp::R16G16B16A16_SNORM:
            return RenderFormat::R16G16B16A16_SNORM;
        case ddspp::R16G16B16A16_SINT:
            return RenderFormat::R16G16B16A16_SINT;
        case ddspp::R32G32_TYPELESS:
            return RenderFormat::R32G32_TYPELESS;
        case ddspp::R32G32_FLOAT:
            return RenderFormat::R32G32_FLOAT;
        case ddspp::R32G32_UINT:
            return RenderFormat::R32G32_UINT;
        case ddspp::R32G32_SINT:
            return RenderFormat::R32G32_SINT;
        case ddspp::R8G8B8A8_TYPELESS:
            return RenderFormat::R8G8B8A8_TYPELESS;
        case ddspp::R8G8B8A8_UNORM:
            return RenderFormat::R8G8B8A8_UNORM;
        case ddspp::R8G8B8A8_UINT:
            return RenderFormat::R8G8B8A8_UINT;
        case ddspp::R8G8B8A8_SNORM:
            return RenderFormat::R8G8B8A8_SNORM;
        case ddspp::R8G8B8A8_SINT:
            return RenderFormat::R8G8B8A8_SINT;
        case ddspp::B8G8R8A8_UNORM:
            return RenderFormat::B8G8R8A8_UNORM;
        case ddspp::R16G16_TYPELESS:
            return RenderFormat::R16G16_TYPELESS;
        case ddspp::R16G16_FLOAT:
            return RenderFormat::R16G16_FLOAT;
        case ddspp::R16G16_UNORM:
            return RenderFormat::R16G16_UNORM;
        case ddspp::R16G16_UINT:
            return RenderFormat::R16G16_UINT;
        case ddspp::R16G16_SNORM:
            return RenderFormat::R16G16_SNORM;
        case ddspp::R16G16_SINT:
            return RenderFormat::R16G16_SINT;
        case ddspp::R32_TYPELESS:
            return RenderFormat::R32_TYPELESS;
        case ddspp::D32_FLOAT:
            return RenderFormat::D32_FLOAT;
        case ddspp::R32_FLOAT:
            return RenderFormat::R32_FLOAT;
        case ddspp::R32_UINT:
            return RenderFormat::R32_UINT;
        case ddspp::R32_SINT:
            return RenderFormat::R32_SINT;
        case ddspp::R8G8_TYPELESS:
            return RenderFormat::R8G8_TYPELESS;
        case ddspp::R8G8_UNORM:
            return RenderFormat::R8G8_UNORM;
        case ddspp::R8G8_UINT:
            return RenderFormat::R8G8_UINT;
        case ddspp::R8G8_SNORM:
            return RenderFormat::R8G8_SNORM;
        case ddspp::R8G8_SINT:
            return RenderFormat::R8G8_SINT;
        case ddspp::R16_TYPELESS:
            return RenderFormat::R16_TYPELESS;
        case ddspp::R16_FLOAT:
            return RenderFormat::R16_FLOAT;
        case ddspp::D16_UNORM:
            return RenderFormat::D16_UNORM;
        case ddspp::R16_UNORM:
            return RenderFormat::R16_UNORM;
        case ddspp::R16_UINT:
            return RenderFormat::R16_UINT;
        case ddspp::R16_SNORM:
            return RenderFormat::R16_SNORM;
        case ddspp::R16_SINT:
            return RenderFormat::R16_SINT;
        case ddspp::R8_TYPELESS:
            return RenderFormat::R8_TYPELESS;
        case ddspp::R8_UNORM:
            return RenderFormat::R8_UNORM;
        case ddspp::R8_UINT:
            return RenderFormat::R8_UINT;
        case ddspp::R8_SNORM:
            return RenderFormat::R8_SNORM;
        case ddspp::R8_SINT:
            return RenderFormat::R8_SINT;
        case ddspp::BC1_TYPELESS:
            return RenderFormat::BC1_TYPELESS;
        case ddspp::BC1_UNORM:
            return RenderFormat::BC1_UNORM;
        case ddspp::BC1_UNORM_SRGB:
            return RenderFormat::BC1_UNORM_SRGB;
        case ddspp::BC2_TYPELESS:
            return RenderFormat::BC2_TYPELESS;
        case ddspp::BC2_UNORM:
            return RenderFormat::BC2_UNORM;
        case ddspp::BC2_UNORM_SRGB:
            return RenderFormat::BC2_UNORM_SRGB;
        case ddspp::BC3_TYPELESS:
            return RenderFormat::BC3_TYPELESS;
        case ddspp::BC3_UNORM:
            return RenderFormat::BC3_UNORM;
        case ddspp::BC3_UNORM_SRGB:
            return RenderFormat::BC3_UNORM_SRGB;
        case ddspp::BC4_TYPELESS:
            return RenderFormat::BC4_TYPELESS;
        case ddspp::BC4_UNORM:
            return RenderFormat::BC4_UNORM;
        case ddspp::BC4_SNORM:
            return RenderFormat::BC4_SNORM;
        case ddspp::BC5_TYPELESS:
            return RenderFormat::BC5_TYPELESS;
        case ddspp::BC5_UNORM:
            return RenderFormat::BC5_UNORM;
        case ddspp::BC5_SNORM:
            return RenderFormat::BC5_SNORM;
        case ddspp::BC6H_TYPELESS:
            return RenderFormat::BC6H_TYPELESS;
        case ddspp::BC6H_UF16:
            return RenderFormat::BC6H_UF16;
        case ddspp::BC6H_SF16:
            return RenderFormat::BC6H_SF16;
        case ddspp::BC7_TYPELESS:
            return RenderFormat::BC7_TYPELESS;
        case ddspp::BC7_UNORM:
            return RenderFormat::BC7_UNORM;
        case ddspp::BC7_UNORM_SRGB:
            return RenderFormat::BC7_UNORM_SRGB;
        default:
            assert(false && "Unsupported format from DDS.");
            return RenderFormat::UNKNOWN;
        }
    }

    bool TextureCache::setDDS(Texture *dstTexture, RenderDevice *device, RenderCommandList *commandList, const uint8_t *bytes, size_t byteCount, std::unique_ptr<RenderBuffer> &dstUploadResource, RenderPool *uploadResourcePool, std::mutex *uploadResourcePoolMutex) {
        assert(dstTexture != nullptr);
        assert(device != nullptr);
        assert(commandList != nullptr);
        assert(bytes != nullptr);

        ddspp::Descriptor ddsDescriptor;
        ddspp::Result result = ddspp::decode_header((unsigned char *)(bytes), ddsDescriptor);
        if (result != ddspp::Success) {
            return false;
        }

        assert(ddsDescriptor.arraySize == 1 && "DDS with multiple arrays are not supported yet.");

        // Retrieve the block size of the format.
        uint32_t blockWidth, blockHeight;
        ddspp::get_block_size(ddsDescriptor.format, blockWidth, blockHeight);

        RenderTextureDesc desc;
        desc.dimension = toRenderDimension(ddsDescriptor.type);
        desc.width = nextSizeAlignedTo(ddsDescriptor.width, blockWidth);
        desc.height = nextSizeAlignedTo(ddsDescriptor.height, blockHeight);
        desc.depth = 1;
        desc.arraySize = 1;
        desc.mipLevels = ddsDescriptor.numMips;
        desc.format = toRenderFormat(ddsDescriptor.format);

        dstTexture->texture = device->createTexture(desc);
        dstTexture->width = desc.width;
        dstTexture->height = desc.height;
        dstTexture->mipmaps = desc.mipLevels;
        dstTexture->format = desc.format;

        const uint8_t *imageData = &bytes[ddsDescriptor.headerSize];
        size_t imageDataSize = byteCount - ddsDescriptor.headerSize;

        // Compute the additional padding that will be required on the buffer to align the mipmap data.
        std::vector<uint32_t> mipmapOffsets;
        const uint32_t formatSize = RenderFormatSize(desc.format);
        uint32_t totalSize = 0;
        for (uint32_t mip = 0; mip < desc.mipLevels; mip++) {
            totalSize = nextSizeAlignedTo(totalSize, TextureDataPlacementAlignment);
            mipmapOffsets.emplace_back(totalSize);

            uint32_t mipHeight = std::max(desc.height >> mip, 1U);
            uint32_t ddsRowPitch = ddspp::get_row_pitch(ddsDescriptor, mip);
            uint32_t alignedRowPitch = nextSizeAlignedTo(ddsRowPitch, TextureDataPitchAlignment);
            uint32_t rowCount = (mipHeight + blockWidth - 1) / blockWidth;
            totalSize += alignedRowPitch * rowCount;
        }

        if (uploadResourcePool != nullptr) {
            assert(uploadResourcePoolMutex != nullptr);
            std::unique_lock queueLock(*uploadResourcePoolMutex);
            dstUploadResource = uploadResourcePool->createBuffer(RenderBufferDesc::UploadBuffer(totalSize));
        }
        else {
            dstUploadResource = device->createBuffer(RenderBufferDesc::UploadBuffer(totalSize));
        }

        dstTexture->memorySize = totalSize;

        // Copy each mipmap into the buffer with the correct padding applied.
        uint8_t *dstData = reinterpret_cast<uint8_t *>(dstUploadResource->map());
        memset(dstData, 0, totalSize);
        for (uint32_t mip = 0; mip < desc.mipLevels; mip++) {
            uint32_t mipOffset = mipmapOffsets[mip];
            uint32_t mipHeight = std::max(desc.height >> mip, 1U);
            uint32_t ddsOffset = ddspp::get_offset(ddsDescriptor, mip, 0);
            uint32_t ddsRowPitch = ddspp::get_row_pitch(ddsDescriptor, mip);
            uint32_t alignedRowPitch = nextSizeAlignedTo(ddsRowPitch, TextureDataPitchAlignment);
            uint32_t rowCount = (mipHeight + blockWidth - 1) / blockWidth;
            memcpyRows(&dstData[mipOffset], alignedRowPitch, &imageData[ddsOffset], ddsRowPitch, rowCount);
        }

        dstUploadResource->unmap();

        commandList->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(dstTexture->texture.get(), RenderTextureLayout::COPY_DEST));

        for (uint32_t mip = 0; mip < desc.mipLevels; mip++) {
            uint32_t offset = mipmapOffsets[mip];
            uint32_t mipWidth = std::max(desc.width >> mip, 1U);
            uint32_t mipHeight = std::max(desc.height >> mip, 1U);
            uint32_t ddsRowPitch = ddspp::get_row_pitch(ddsDescriptor, mip);
            uint32_t alignedRowWidth = ((nextSizeAlignedTo(ddsRowPitch, TextureDataPitchAlignment) + formatSize - 1) / formatSize) * blockWidth;
            commandList->copyTextureRegion(RenderTextureCopyLocation::Subresource(dstTexture->texture.get(), mip), RenderTextureCopyLocation::PlacedFootprint(dstUploadResource.get(), desc.format, mipWidth, mipHeight, 1, alignedRowWidth, offset));
        }

        return true;
    }

    bool TextureCache::setLowMipCache(RenderDevice *device, RenderCommandList *commandList, const uint8_t *bytes, size_t byteCount, std::unique_ptr<RenderBuffer> &dstUploadResource, std::unordered_map<std::string, LowMipCacheTexture> &dstTextureMap, uint64_t &totalMemory) {
        dstUploadResource = device->createBuffer(RenderBufferDesc::UploadBuffer(byteCount));

        // Upload the entire file to the GPU to copy data from it directly.
        void *uploadData = dstUploadResource->map();
        memcpy(uploadData, bytes, byteCount);
        dstUploadResource->unmap();

        std::vector<RenderTextureBarrier> beforeCopyBarriers;
        std::vector<RenderTextureCopyLocation> copyDestinations;
        std::vector<RenderTextureCopyLocation> copySources;
        std::list<std::pair<std::string, Texture *>> texturesLoaded;
        size_t byteCursor = 0;
        bool readFailed = false;
        while (byteCursor < byteCount) {
            const ReplacementMipmapCacheHeader *cacheHeader = reinterpret_cast<const ReplacementMipmapCacheHeader *>(&bytes[byteCursor]);
            byteCursor += sizeof(ReplacementMipmapCacheHeader);

            if (cacheHeader->magic != ReplacementMipmapCacheHeaderMagic) {
                readFailed = true;
                break;
            }

            if (cacheHeader->version > ReplacementMipmapCacheHeaderVersion) {
                readFailed = true;
                break;
            }

            const uint32_t *mipmapSizes = reinterpret_cast<const uint32_t *>(&bytes[byteCursor]);
            byteCursor += cacheHeader->mipCount * sizeof(uint32_t);

            const uint32_t *mipmapAlignedRowPitches = reinterpret_cast<const uint32_t *>(&bytes[byteCursor]);
            byteCursor += cacheHeader->mipCount * sizeof(uint32_t);

            std::string cachePath(reinterpret_cast<const char *>(&bytes[byteCursor]), cacheHeader->pathLength);
            byteCursor += cacheHeader->pathLength;

            auto alignToTexturePlacement = [](size_t &byteCursor) {
                if ((byteCursor % TextureDataPlacementAlignment) != 0) {
                    byteCursor += TextureDataPlacementAlignment - (byteCursor % TextureDataPlacementAlignment);
                }
            };

            std::string cachePathForward = FileSystem::toForwardSlashes(cachePath);
            const bool skipTexture = (dstTextureMap.find(cachePathForward) != dstTextureMap.end());
            if (skipTexture) {
                for (uint32_t i = 0; i < cacheHeader->mipCount; i++) {
                    alignToTexturePlacement(byteCursor);
                    byteCursor += mipmapSizes[i];
                }
            }
            else {
                RenderFormat renderFormat = toRenderFormat(ddspp::DXGIFormat(cacheHeader->dxgiFormat));
                RenderTextureDesc textureDesc = RenderTextureDesc::Texture2D(cacheHeader->width, cacheHeader->height, cacheHeader->mipCount, renderFormat);
                Texture *newTexture = new Texture();
                newTexture->texture = device->createTexture(textureDesc);
                newTexture->format = renderFormat;
                newTexture->width = cacheHeader->width;
                newTexture->height = cacheHeader->height;
                newTexture->mipmaps = cacheHeader->mipCount;

                const uint32_t formatSize = RenderFormatSize(renderFormat);
                const uint32_t blockWidth = RenderFormatBlockWidth(renderFormat);
                for (uint32_t i = 0; i < cacheHeader->mipCount; i++) {
                    alignToTexturePlacement(byteCursor);
                    uint32_t mipmapWidth = std::max(cacheHeader->width >> i, 1U);
                    uint32_t mipmapHeight = std::max(cacheHeader->height >> i, 1U);
                    uint32_t alignedRowWidth = ((mipmapAlignedRowPitches[i] + formatSize - 1) / formatSize) * blockWidth;
                    beforeCopyBarriers.emplace_back(RenderTextureBarrier(newTexture->texture.get(), RenderTextureLayout::COPY_DEST));
                    copyDestinations.emplace_back(RenderTextureCopyLocation::Subresource(newTexture->texture.get(), i));
                    copySources.emplace_back(RenderTextureCopyLocation::PlacedFootprint(dstUploadResource.get(), renderFormat, mipmapWidth, mipmapHeight, 1, alignedRowWidth, byteCursor));
                    byteCursor += mipmapSizes[i];
                    newTexture->memorySize += mipmapSizes[i];
                }

                totalMemory += newTexture->memorySize;
                texturesLoaded.emplace_back(cachePathForward, newTexture);
            }
        }

        if (readFailed) {
            // Delete all textures that were loaded.
            for (auto pair : texturesLoaded) {
                delete pair.second;
            }

            return false;
        }

        if (!texturesLoaded.empty()) {
            // Only add textures to the map if reading the entire cache was successful.
            for (auto pair : texturesLoaded) {
                dstTextureMap[pair.first] = { pair.second, false };
            }

            // Record to the command list.
            commandList->barriers(RenderBarrierStage::COPY, beforeCopyBarriers);

            for (size_t i = 0; i < copyDestinations.size(); i++) {
                commandList->copyTextureRegion(copyDestinations[i], copySources[i]);
            }
        }

        return true;
    }

    Texture *TextureCache::loadTextureFromBytes(RenderDevice *device, RenderCommandList *commandList, const std::vector<uint8_t> &fileBytes, std::unique_ptr<RenderBuffer> &dstUploadResource, RenderPool *resourcePool, std::mutex *uploadResourcePoolMutex) {
        const uint32_t PNG_MAGIC = 0x474E5089;
        Texture *replacementTexture = new Texture();
        uint32_t magicNumber = *reinterpret_cast<const uint32_t *>(fileBytes.data());
        bool loadedTexture = false;
        switch (magicNumber) {
        case ddspp::DDS_MAGIC:
            loadedTexture = TextureCache::setDDS(replacementTexture, device, commandList, fileBytes.data(), fileBytes.size(), dstUploadResource, resourcePool, uploadResourcePoolMutex);
            break;
        case PNG_MAGIC: {
            int width, height;
            stbi_uc *data = stbi_load_from_memory(fileBytes.data(), fileBytes.size(), &width, &height, nullptr, 4);
            if (data != nullptr) {
                uint32_t rowPitch = uint32_t(width) * 4;
                size_t byteCount = uint32_t(height) * rowPitch;
                TextureCache::setRGBA32(replacementTexture, device, commandList, data, byteCount, uint32_t(width), uint32_t(height), rowPitch, dstUploadResource, resourcePool);
                stbi_image_free(data);
                loadedTexture = true;
            }

            break;
        }
        default:
            // Unknown format.
            break;
        }

        if (loadedTexture) {
            return replacementTexture;
        }
        else {
            delete replacementTexture;
            return nullptr;
        }
    }

    void TextureCache::uploadThreadLoop() {
        Thread::setCurrentThreadName("RT64 Texture");

        uploadThreadRunning = true;

        std::vector<TextureUpload> queueCopy;
        std::vector<TextureUpload> newQueue;
        std::vector<ReplacementResolvedPath> resolvedPathQueueCopy;
        std::vector<StreamResult> streamResultQueueCopy;
        std::vector<TextureMapAddition> textureMapAdditions;
        std::vector<ReplacementMapAddition> replacementMapAdditions;
        std::vector<RenderTextureBarrier> beforeCopyBarriers;
        std::vector<RenderTextureBarrier> beforeDecodeBarriers;
        std::vector<RenderTextureBarrier> afterDecodeBarriers;
        std::vector<uint8_t> replacementBytes;

        while (uploadThreadRunning) {
            resolvedPathQueueCopy.clear();
            streamResultQueueCopy.clear();
            beforeCopyBarriers.clear();
            beforeDecodeBarriers.clear();
            afterDecodeBarriers.clear();

            // Check the top of the queue or wait if it's empty.
            {
                std::unique_lock queueLock(uploadQueueMutex);
                uploadQueueChanged.wait(queueLock, [this]() {
                    return !uploadThreadRunning || !uploadQueue.empty() || !resolvedPathQueue.empty() || !streamResultQueue.empty();
                });

                if (!uploadQueue.empty()) {
                    queueCopy = uploadQueue;
                }

                if (!resolvedPathQueue.empty()) {
                    resolvedPathQueueCopy.insert(resolvedPathQueueCopy.end(), resolvedPathQueue.begin(), resolvedPathQueue.end());
                    resolvedPathQueue.clear();
                }

                if (!streamResultQueue.empty()) {
                    streamResultQueueCopy.insert(streamResultQueueCopy.end(), streamResultQueue.begin(), streamResultQueue.end());
                    streamResultQueue.clear();
                }
            }
            
            if (!streamResultQueueCopy.empty()) {
                {
                    // Add the textures to the replacement pool as a loaded texture.
                    std::unique_lock lock(textureMapMutex);
                    for (const StreamResult &result : streamResultQueueCopy) {
                        textureMap.replacementMap.addLoadedTexture(result.texture, result.fileSystemIndex, result.relativePath, !result.fromPreload);

                        // Increment texture pool memory used permanently if the texture was preloaded.
                        if (result.fromPreload) {
                            textureMap.replacementMap.usedTexturePoolSize += result.texture->memorySize;
                            textureMap.replacementMap.cachedTexturePoolSize += result.texture->memorySize;
                        }
                    }
                }

                // Add all the pending transition barriers and replacement checks.
                for (const StreamResult &result : streamResultQueueCopy) {
                    afterDecodeBarriers.emplace_back(result.texture->texture.get(), RenderTextureLayout::SHADER_READ);

                    auto &streamResolvedPaths = textureMap.replacementMap.fileSystemStreamResolvedPaths[result.fileSystemIndex];
                    auto it = streamResolvedPaths.find(result.relativePath);
                    while (it != streamResolvedPaths.end()) {
                        resolvedPathQueueCopy.emplace_back(it->second);
                        streamResolvedPaths.erase(it);
                        it = streamResolvedPaths.find(result.relativePath);
                    }
                }
            }

            if (!queueCopy.empty() || !resolvedPathQueueCopy.empty() || !afterDecodeBarriers.empty()) {
                // Create new upload buffers and descriptor heaps to fill out the required size.
                const size_t queueSize = queueCopy.size();
                const uint64_t TMEMSize = 0x1000;
                {
                    std::unique_lock queueLock(uploadResourcePoolMutex);
                    for (size_t i = tmemUploadResources.size(); i < queueSize; i++) {
                        tmemUploadResources.emplace_back(uploadResourcePool->createBuffer(RenderBufferDesc::UploadBuffer(TMEMSize)));
                    }
                }

                for (size_t i = descriptorSets.size(); i < queueSize; i++) {
                    descriptorSets.emplace_back(std::make_unique<TextureDecodeDescriptorSet>(directWorker->device));
                }

                // Upload all textures in the queue using the copy worker. It's worth noting the usage of a copy worker during copy texture operations is intentional
                // to avoid issues with drivers that are not very explicit about where they execute the copy texture operations and end up causing subtle synchronization
                // issues that can't be accounted for. Using dedicated copy queues for these operations has shown to eliminate these issues entirely.
                copyWorker->commandList->begin();
                textureMapAdditions.clear();

                for (size_t i = 0; i < queueSize; i++) {
                    static uint32_t TMEMGlobalCounter = 0;
                    const TextureUpload &upload = queueCopy[i];
                    Texture *newTexture = new Texture();
                    newTexture->creationFrame = upload.creationFrame;
                    textureMapAdditions.emplace_back(TextureMapAddition{ upload.hash, newTexture });

                    newTexture->format = RenderFormat::R8_UINT;
                    newTexture->width = upload.width;
                    newTexture->height = upload.height;
                    newTexture->tmem = copyWorker->device->createTexture(RenderTextureDesc::Texture1D(std::max(uint32_t(upload.bytesTMEM.size()), 1U), 1, newTexture->format));
                    newTexture->tmem->setName("Texture Cache TMEM #" + std::to_string(TMEMGlobalCounter++));
                    newTexture->bytesTMEM = upload.bytesTMEM;
                    newTexture->loadTile = upload.loadTile;
                    newTexture->tlut = upload.tlut;
                    newTexture->decodeTMEM = upload.decodeTMEM;

                    if (!upload.bytesTMEM.empty()) {
                        void *dstData = tmemUploadResources[i]->map();
                        memcpy(dstData, upload.bytesTMEM.data(), upload.bytesTMEM.size());
                        tmemUploadResources[i]->unmap();
                    }

                    beforeCopyBarriers.emplace_back(newTexture->tmem.get(), RenderTextureLayout::COPY_DEST);
                }

                copyWorker->commandList->barriers(RenderBarrierStage::COPY, beforeCopyBarriers);

                for (size_t i = 0; i < queueSize; i++) {
                    const TextureUpload &upload = queueCopy[i];
                    const uint32_t byteCount = uint32_t(upload.bytesTMEM.size());
                    Texture *dstTexture = textureMapAdditions[i].texture;
                    if (byteCount > 0) {
                        copyWorker->commandList->copyTextureRegion(
                            RenderTextureCopyLocation::Subresource(dstTexture->tmem.get()),
                            RenderTextureCopyLocation::PlacedFootprint(tmemUploadResources[i].get(), RenderFormat::R8_UINT, byteCount, 1, 1, byteCount)
                        );
                    }

                    beforeDecodeBarriers.emplace_back(dstTexture->tmem.get(), RenderTextureLayout::SHADER_READ);

                    if (upload.decodeTMEM) {
                        static uint32_t TextureGlobalCounter = 0;
                        TextureDecodeDescriptorSet *descSet = descriptorSets[i].get();
                        dstTexture->format = RenderFormat::R8G8B8A8_UNORM;
                        dstTexture->texture = directWorker->device->createTexture(RenderTextureDesc::Texture2D(upload.width, upload.height, 1, dstTexture->format, RenderTextureFlag::STORAGE | RenderTextureFlag::UNORDERED_ACCESS));
                        dstTexture->texture->setName("Texture Cache RGBA32 #" + std::to_string(TextureGlobalCounter++));
                        descSet->setTexture(descSet->TMEM, dstTexture->tmem.get(), RenderTextureLayout::SHADER_READ);
                        descSet->setTexture(descSet->RGBA32, dstTexture->texture.get(), RenderTextureLayout::GENERAL);
                        beforeDecodeBarriers.emplace_back(dstTexture->texture.get(), RenderTextureLayout::GENERAL);
                    }

                    addResolvedPaths(upload.hash, upload.width, upload.height, upload.tlut, upload.loadTile, upload.bytesTMEM, upload.decodeTMEM, resolvedPathQueueCopy);
                }

                replacementMapAdditions.clear();
                for (const ReplacementResolvedPath &resolvedPath : resolvedPathQueueCopy) {
                    Texture *replacementTexture = textureMap.replacementMap.getFromRelativePath(resolvedPath.fileSystemIndex, resolvedPath.relativePath);
                    Texture *lowMipCacheTexture = nullptr;

                    // Look for the low mip cache version if it exists if we can't use the real replacement yet.
                    if ((replacementTexture == nullptr) && (resolvedPath.resolvedOperation == ReplacementOperation::Stream)) {
                        auto lowMipCacheIt = textureMap.replacementMap.lowMipCacheTextures.find(resolvedPath.relativePath);
                        if (lowMipCacheIt != textureMap.replacementMap.lowMipCacheTextures.end()) {
                            lowMipCacheTexture = lowMipCacheIt->second.texture;

                            // Transition the texture from the low mip cache if it hasn't been transitioned to shader read yet.
                            if (!lowMipCacheIt->second.transitioned) {
                                afterDecodeBarriers.emplace_back(lowMipCacheTexture->texture.get(), RenderTextureLayout::SHADER_READ);
                                lowMipCacheIt->second.transitioned = true;
                            }
                        }
                    }

                    // Replacement texture hasn't been loaded yet.
                    if (replacementTexture == nullptr) {
                        // Queue the texture for being loaded from a texture cache streaming thread.
                        if (resolvedPath.resolvedOperation == ReplacementOperation::Stream) {
#                           if !ONLY_USE_LOW_MIP_CACHE
                            // Make sure the replacement map hasn't queued or loaded the relative path already.
                            auto &streamSet = textureMap.replacementMap.fileSystemStreamSets[resolvedPath.fileSystemIndex];
                            if (streamSet.find(resolvedPath.relativePath) == streamSet.end()) {
                                streamSet.insert(resolvedPath.relativePath);

                                // Push to the streaming queue.
                                streamDescStackMutex.lock();
                                streamDescStack.push(StreamDescription(resolvedPath.fileSystemIndex, resolvedPath.relativePath, false));
                                streamDescStackMutex.unlock();
                                streamDescStackChanged.notify_all();
                            }
#                           endif

                            // Store the hash to check for the replacement when the texture is done streaming.
                            textureMap.replacementMap.fileSystemStreamResolvedPaths[resolvedPath.fileSystemIndex].emplace(resolvedPath.relativePath, resolvedPath);

                            // Use the low mip cache texture if it exists.
                            replacementTexture = lowMipCacheTexture;
                        }
                        // Load the texture directly on this thread (operation was defined as Stall).
                        else if (textureMap.replacementMap.fileSystems[resolvedPath.fileSystemIndex]->load(resolvedPath.relativePath, replacementBytes)) {
                            replacementUploadResources.emplace_back();
                            replacementTexture = TextureCache::loadTextureFromBytes(copyWorker->device, copyWorker->commandList.get(), replacementBytes, replacementUploadResources.back());
                            textureMapMutex.lock();
                            textureMap.replacementMap.addLoadedTexture(replacementTexture, resolvedPath.fileSystemIndex, resolvedPath.relativePath, true);
                            textureMapMutex.unlock();
                            afterDecodeBarriers.emplace_back(replacementTexture->texture.get(), RenderTextureLayout::SHADER_READ);
                        }
                    }

                    if (replacementTexture != nullptr) {
                        // We don't use reference counting on preloaded textures or low mip cache versions, as they're permanently allocated in memory.
                        bool referenceCounted = (resolvedPath.resolvedOperation != ReplacementOperation::Preload) && (replacementTexture != lowMipCacheTexture);
                        replacementMapAdditions.emplace_back(ReplacementMapAddition{ resolvedPath.textureHash, replacementTexture, resolvedPath.resolvedShift, referenceCounted });
                    }
                }
                
                // Execute the copy worker and signal a semaphore so it synchronizes with the direct worker afterwards.
                const RenderCommandList *copyCommandList = copyWorker->commandList.get();
                RenderCommandSemaphore *syncSemaphore = copyToDirectSemaphore.get();
                copyWorker->commandList->end();
                copyWorker->commandQueue->executeCommandLists(&copyCommandList, 1, nullptr, 0, &syncSemaphore, 1);
                
                // Decode all textures using the direct worker and transition all textures to their final layouts.
                directWorker->commandList->begin();
                directWorker->commandList->barriers(RenderBarrierStage::GRAPHICS_AND_COMPUTE, beforeDecodeBarriers);

                const ShaderRecord &textureDecode = shaderLibrary->textureDecode;
                bool pipelineSet = false;
                for (size_t i = 0; i < queueSize; i++) {
                    const TextureUpload &upload = queueCopy[i];
                    if (upload.decodeTMEM) {
                        if (!pipelineSet) {
                            directWorker->commandList->setPipeline(textureDecode.pipeline.get());
                            directWorker->commandList->setComputePipelineLayout(textureDecode.pipelineLayout.get());
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
                        directWorker->commandList->setComputePushConstants(0, &decodeCB);
                        directWorker->commandList->setComputeDescriptorSet(descriptorSets[i]->get(), 0);
                        directWorker->commandList->dispatch(dispatchX, dispatchY, 1);

                        afterDecodeBarriers.emplace_back(RenderTextureBarrier(textureMapAdditions[i].texture->texture.get(), RenderTextureLayout::SHADER_READ));
                    }
                }

                if (!afterDecodeBarriers.empty()) {
                    directWorker->commandList->barriers(RenderBarrierStage::GRAPHICS_AND_COMPUTE, afterDecodeBarriers);
                }

                // Execute the direct worker but make it wait for the copy worker to finish first.
                const RenderCommandList *directCommandList = directWorker->commandList.get();
                directWorker->commandList->end();
                directWorker->commandQueue->executeCommandLists(&directCommandList, 1, &syncSemaphore, 1, nullptr, 0, directWorker->commandFence.get());
                directWorker->wait();

                // Delete all the resources used during the upload of replacements.
                replacementUploadResources.clear();
                
                // Add all the textures to the map once they're ready.
                {
                    std::unique_lock lock(textureMapMutex);
                    for (const TextureMapAddition &addition : textureMapAdditions) {
                        textureMap.add(addition.hash, addition.texture->creationFrame, addition.texture);
                    }

                    for (const ReplacementMapAddition &addition : replacementMapAdditions) {
                        textureMap.replace(addition.hash, addition.texture, addition.shift == ReplacementShift::Half, addition.referenceCounted);
                    }

                    textureMap.replacementMap.evict(textureMap.evictedTextures);
                }

                // Make the new queue the remaining subsection of the upload queue that wasn't processed in this batch.
                {
                    std::unique_lock queueLock(uploadQueueMutex);
                    newQueue = std::vector<TextureUpload>(uploadQueue.begin() + queueSize, uploadQueue.end());
                    uploadQueue = std::move(newQueue);
                }

                queueCopy.clear();
                uploadQueueFinished.notify_all();
            }
        }
    }

    void TextureCache::queueGPUUploadTMEM(uint64_t hash, uint64_t creationFrame, const uint8_t *bytes, int bytesCount, int width, int height, uint32_t tlut, const LoadTile &loadTile, bool decodeTMEM) {
        assert(!decodeTMEM || ((width > 0) && (height > 0)));

        TextureUpload newUpload;
        newUpload.hash = hash;
        newUpload.creationFrame = creationFrame;
        newUpload.width = width;
        newUpload.height = height;
        newUpload.tlut = tlut;
        newUpload.loadTile = loadTile;
        newUpload.bytesTMEM = (bytes != nullptr) ? std::vector<uint8_t>(bytes, bytes + bytesCount) : std::vector<uint8_t>();
        newUpload.decodeTMEM = decodeTMEM;

        {
            std::unique_lock queueLock(uploadQueueMutex);
            uploadQueue.emplace_back(newUpload);
        }

        uploadQueueChanged.notify_all();
    }

    void TextureCache::waitForGPUUploads() {
        std::unique_lock queueLock(uploadQueueMutex);
        uploadQueueFinished.wait(queueLock, [this]() {
            return uploadQueue.empty();
        });
    }

    void TextureCache::addResolvedPaths(uint64_t hash, uint32_t width, uint32_t height, uint32_t tlut, const LoadTile &loadTile, const std::vector<uint8_t> &bytesTMEM, bool decodeTMEM, std::vector<ReplacementResolvedPath> &resolvedPaths, uint64_t exclusiveDbHash) {
        uint64_t hashes[TMEMHasher::CurrentHashVersion + 1] = {};
        for (uint32_t v : textureMap.replacementMap.resolvedHashVersions) {
            if (decodeTMEM && v < TMEMHasher::CurrentHashVersion) {
                // If the database uses an older hash version, we hash TMEM again with the version corresponding to the database.
                hashes[v] = TMEMHasher::hash(bytesTMEM.data(), loadTile, width, height, tlut, v);
            }
            else {
                hashes[v] = hash;
            }
        }

        for (int32_t i = int32_t(textureMap.replacementMap.fileSystemResolvedPaths.size()) - 1; i >= 0; i--) {
            const auto &fsResolvedPaths = textureMap.replacementMap.fileSystemResolvedPaths[i];
            uint32_t hashVersion = textureMap.replacementMap.fileSystemHashVersions[i];
            if ((exclusiveDbHash == 0) || (exclusiveDbHash == hashes[hashVersion])) {
                auto it = fsResolvedPaths.find(hashes[hashVersion]);
                if (it != fsResolvedPaths.end()) {
                    // Stop searching for replacements as soon as we find one.
                    resolvedPaths.emplace_back(it->second);
                    break;
                }
            }
        }
    }

    bool TextureCache::useTexture(uint64_t hash, uint64_t submissionFrame, uint32_t &textureIndex, interop::float2 &textureScale, interop::float3 &textureDimensions, bool &textureReplaced, bool &hasMipmaps, bool &shiftedByHalf) {
        std::unique_lock lock(textureMapMutex);
        return textureMap.use(hash, submissionFrame, textureIndex, textureScale, textureDimensions, textureReplaced, hasMipmaps, shiftedByHalf);
    }

    bool TextureCache::useTexture(uint64_t hash, uint64_t submissionFrame, uint32_t &textureIndex) {
        interop::float2 textureScale;
        interop::float3 textureDimensions;
        bool textureReplaced;
        bool hasMipmaps;
        bool shiftedByHalf;
        return useTexture(hash, submissionFrame, textureIndex, textureScale, textureDimensions, textureReplaced, hasMipmaps, shiftedByHalf);
    }
    
    bool TextureCache::addReplacement(uint64_t hash, const std::string &relativePath, ReplacementShift shift) {
        waitForAllStreamThreads(false);
        waitForGPUUploads();

        std::unique_lock lock(textureMapMutex);
        if (!textureMap.replacementMap.fileSystemIsDirectory) {
            return false;
        }

        std::vector<uint8_t> replacementBytes;
        if (!textureMap.replacementMap.fileSystems.front()->load(relativePath, replacementBytes)) {
            return false;
        }

        // Load texture replacement immediately.
        bool loadedNewTexture = false;
        std::string relativePathForward = FileSystem::toForwardSlashes(relativePath);
        Texture *newTexture = textureMap.replacementMap.getFromRelativePath(0, relativePathForward);
        if (newTexture == nullptr) {
            std::unique_ptr<RenderBuffer> dstUploadBuffer;
            loaderCommandList->begin();
            newTexture = TextureCache::loadTextureFromBytes(copyWorker->device, loaderCommandList.get(), replacementBytes, dstUploadBuffer);
            loaderCommandList->end();

            if (newTexture != nullptr) {
                copyWorker->commandQueue->executeCommandLists(loaderCommandList.get(), loaderCommandFence.get());
                copyWorker->commandQueue->waitForCommandFence(loaderCommandFence.get());
            }

            loadedNewTexture = true;
        }

        if (newTexture == nullptr) {
            return false;
        }
        
        // Store replacement in the replacement database.
        ReplacementTexture replacement;
        replacement.hashes.rt64 = ReplacementDatabase::hashToString(hash);
        replacement.path = ReplacementDatabase::removeKnownExtension(relativePathForward);
        replacement.shift = shift;

        ReplacementDatabase &replacementDb = textureMap.replacementMap.directoryDatabase;
        replacementDb.addReplacement(replacement);
        
        ReplacementOperation resolvedOperation = replacementDb.resolveOperation(replacement.path, replacement.operation);
        ReplacementShift resolvedShift = replacementDb.resolveShift(replacement.path, replacement.shift);
        ReplacementResolvedPath resolvedPath = { 0, hash, relativePathForward, resolvedOperation, replacement.operation, resolvedShift, replacement.shift };
        textureMap.replacementMap.fileSystemResolvedPaths[0][hash] = resolvedPath;

        uploadQueueMutex.lock();

        // Queue the texture as if it was the result from a streaming thread.
        if (loadedNewTexture) {
            streamResultQueue.emplace_back(newTexture, 0, relativePathForward, false);
            textureMap.replacementMap.fileSystemStreamResolvedPaths[0].emplace(relativePathForward, resolvedPath);
        }
        // Just queue a replacement check for the hash that was just replaced.
        else {
            resolvedPathQueue.emplace_back(resolvedPath);
        }

        uploadQueueMutex.unlock();
        uploadQueueChanged.notify_all();

        return true;
    }

    bool TextureCache::hasReplacement(uint64_t hash) {
        std::unique_lock lock(textureMapMutex);
        const auto &resolvedPaths = textureMap.replacementMap.fileSystemResolvedPaths;
        for (int32_t i = int32_t(resolvedPaths.size()) - 1; i >= 0; i--) {
            if (resolvedPaths[i].find(hash) != resolvedPaths[i].end()) {
                return true;
            }
        }
        
        return false;
    }

    void TextureCache::clearReplacementDirectories() {
        // Wait for the streaming threads to be finished.
        waitForAllStreamThreads(true);

        // Reset the benchmark counters.
        resetStreamPerformanceCounters();

        // Flush the current stream result queue and delete all the textures in it.
        {
            std::unique_lock queueLock(uploadQueueMutex);
            for (const StreamResult &result : streamResultQueue) {
                delete result.texture;
            }

            streamResultQueue.clear();
        }

        // Clear the current set of textures that were sent to streaming threads.
        textureMap.replacementMap.fileSystemStreamSets.clear();

        // Clear the pending replacement checks for streamed textures.
        textureMap.replacementMap.fileSystemStreamResolvedPaths.clear();

        // Lock the texture map and start changing replacements. This function is assumed to be called from the only
        // thread that is capable of submitting new textures and must've waited beforehand for all textures to be uploaded.
        std::unique_lock lock(textureMapMutex);
        textureMap.clearReplacements();
        textureMap.replacementMap.clear(textureMap.evictedTextures);
    }

    void TextureCache::reloadReplacements(bool clearQueue, uint64_t exclusiveDbHash) {
        // Queue all currently loaded hashes to detect replacements with.
        {
            std::unique_lock queueLock(uploadQueueMutex);
            if (clearQueue) {
                resolvedPathQueue.clear();
            }

            for (size_t i = 0; i < textureMap.textures.size(); i++) {
                if (textureMap.textures[i] != nullptr) {
                    Texture *texture = textureMap.textures[i];
                    addResolvedPaths(textureMap.hashes[i], texture->width, texture->height, texture->tlut, texture->loadTile, texture->bytesTMEM, texture->decodeTMEM, resolvedPathQueue, exclusiveDbHash);
                }
            }
        }

        uploadQueueChanged.notify_all();
    }

    bool TextureCache::loadReplacementDirectory(const ReplacementDirectory &replacementDirectory) {
        return loadReplacementDirectories({ replacementDirectory });
    }
    
    bool TextureCache::loadReplacementDirectories(const std::vector<ReplacementDirectory> &replacementDirectories) {
        clearReplacementDirectories();

        std::unique_lock lock(textureMapMutex);
        textureMap.replacementMap.replacementDirectories = replacementDirectories;

        std::vector<std::unique_ptr<FileSystem>> fileSystems;
        std::vector<std::unordered_map<uint64_t, ReplacementResolvedPath>> fileSystemResolvedPaths;
        std::vector<uint32_t> fileSystemHashVersions;
        std::vector<std::unordered_set<std::string>> fileSystemStreamSets;
        for (const ReplacementDirectory &replacementDirectory : replacementDirectories) {
            if (std::filesystem::is_regular_file(replacementDirectory.dirOrZipPath)) {
                std::unique_ptr<FileSystem> fileSystem = FileSystemZip::create(replacementDirectory.dirOrZipPath, replacementDirectory.zipBasePath);
                if (fileSystem == nullptr) {
                    fprintf(stderr, "Failed to load file system as a pack for replacements.\n");
                    return false;
                }

                fileSystems.emplace_back(std::move(fileSystem));
                fileSystemResolvedPaths.emplace_back();
                fileSystemHashVersions.emplace_back();
                fileSystemStreamSets.emplace_back();
            }
            else if (std::filesystem::is_directory(replacementDirectory.dirOrZipPath)) {
                std::unique_ptr<FileSystem> fileSystem = FileSystemDirectory::create(replacementDirectory.dirOrZipPath);
                if (fileSystem == nullptr) {
                    fprintf(stderr, "Failed to load file system as a directory for replacements.\n");
                    return false;
                }

                // Only enable that file system is a directory if it's only one directory.
                textureMap.replacementMap.fileSystemIsDirectory = (replacementDirectories.size() == 1);
                fileSystems.emplace_back(std::move(fileSystem));
                fileSystemResolvedPaths.emplace_back();
                fileSystemHashVersions.emplace_back();
                fileSystemStreamSets.emplace_back();
            }
            else {
                fprintf(stderr, "Failed to identify what type of filesystem the path is.\n");
                return false;
            }
        }

        // Load databases and low mipmap caches from the filesystems in reverse order.
        {
            loaderCommandList->begin();

            uint64_t totalMemory = 0;
            std::vector<uint8_t> databaseBytes;
            std::vector<uint8_t> mipCacheBytes;
            std::vector<std::unique_ptr<RenderBuffer>> uploadBuffers;
            std::set<uint32_t> knownHashVersions;
            for (int32_t i = int32_t(fileSystems.size()) - 1; i >= 0; i--) {
                if (fileSystems[i]->load(ReplacementDatabaseFilename, databaseBytes)) {
                    try {
                        ReplacementDatabase db;
                        db = json::parse(databaseBytes.begin(), databaseBytes.end(), nullptr, true);

                        if (db.config.hashVersion <= TMEMHasher::CurrentHashVersion) {
                            db.resolvePaths(fileSystems[i].get(), uint32_t(i), fileSystemResolvedPaths[i], false, nullptr, &fileSystemStreamSets[i]);
                            fileSystemHashVersions[i] = db.config.hashVersion;
                            knownHashVersions.insert(db.config.hashVersion);

                            if (textureMap.replacementMap.fileSystemIsDirectory) {
                                textureMap.replacementMap.directoryDatabase = std::move(db);
                            }
                        }
                    }
                    catch (const nlohmann::detail::exception &e) {
                        fprintf(stderr, "JSON parsing error: %s\n", e.what());
                    }
                }

                if (fileSystems[i]->load(ReplacementLowMipCacheFilename, mipCacheBytes)) {
                    uploadBuffers.emplace_back();
                    if (!setLowMipCache(copyWorker->device, loaderCommandList.get(), mipCacheBytes.data(), mipCacheBytes.size(), uploadBuffers.back(), textureMap.replacementMap.lowMipCacheTextures, totalMemory)) {
                        fprintf(stderr, "Failed to load low mip cache.\n");
                    }
                }
            }

            loaderCommandList->end();

            if (!textureMap.replacementMap.lowMipCacheTextures.empty()) {
                copyWorker->commandQueue->executeCommandLists(loaderCommandList.get(), loaderCommandFence.get());
                copyWorker->commandQueue->waitForCommandFence(loaderCommandFence.get());

                // Memory used by low mip cache is considered as permanently in used.
                textureMap.replacementMap.usedTexturePoolSize += totalMemory;
                textureMap.replacementMap.cachedTexturePoolSize += totalMemory;
            }

            // Build a vector from the known hash versions.
            textureMap.replacementMap.resolvedHashVersions.clear();
            textureMap.replacementMap.resolvedHashVersions.insert(textureMap.replacementMap.resolvedHashVersions.end(), knownHashVersions.begin(), knownHashVersions.end());
        }

        // Store the file systems in the replacement map.
        uint32_t fileSystemCount = uint32_t(fileSystems.size());
        textureMap.replacementMap.fileSystems = std::move(fileSystems);
        textureMap.replacementMap.fileSystemResolvedPaths = std::move(fileSystemResolvedPaths);
        textureMap.replacementMap.fileSystemHashVersions = std::move(fileSystemHashVersions);
        textureMap.replacementMap.fileSystemStreamSets = std::move(fileSystemStreamSets);
        textureMap.replacementMap.fileSystemStreamResolvedPaths.resize(fileSystemCount);

        // Queue all textures that must be preloaded to the stream queues.
        bool texturesPreloaded = false;
        {
            std::unique_lock queueLock(streamDescStackMutex);
            for (uint32_t i = 0; i < fileSystemCount; i++) {
                for (const std::string &relativePath : textureMap.replacementMap.fileSystemStreamSets[i]) {
                    streamDescStack.push(StreamDescription(i, relativePath, true));
                    texturesPreloaded = true;
                }
            }
        }

        if (texturesPreloaded) {
            streamDescStackChanged.notify_all();

            // Wait for all the streaming threads to be finished.
            waitForAllStreamThreads(false);
        }

        reloadReplacements(true);

        return true;
    }

    bool TextureCache::saveReplacementDatabase() {
        std::unique_lock lock(textureMapMutex);
        if (!textureMap.replacementMap.fileSystemIsDirectory) {
            return false;
        }

        const std::filesystem::path directoryPath = textureMap.replacementMap.replacementDirectories[0].dirOrZipPath;
        const std::filesystem::path databasePath = directoryPath / ReplacementDatabaseFilename;
        const std::filesystem::path databaseNewPath = directoryPath / (ReplacementDatabaseFilename + ".new");
        const std::filesystem::path databaseOldPath = directoryPath / (ReplacementDatabaseFilename + ".old");
        std::ofstream databaseNewFile(databaseNewPath);
        if (!textureMap.replacementMap.saveDatabase(databaseNewFile)) {
            return false;
        }

        databaseNewFile.close();

        std::error_code ec;
        if (std::filesystem::exists(databasePath)) {
            if (std::filesystem::exists(databaseOldPath)) {
                std::filesystem::remove(databaseOldPath, ec);
                if (ec) {
                    fprintf(stderr, "%s\n", ec.message().c_str());
                    return false;
                }
            }

            std::filesystem::rename(databasePath, databaseOldPath, ec);
            if (ec) {
                fprintf(stderr, "%s\n", ec.message().c_str());
                return false;
            }
        }

        std::filesystem::rename(databaseNewPath, databasePath, ec);
        if (ec) {
            fprintf(stderr, "%s\n", ec.message().c_str());
            return false;
        }

        return true;
    }

    void TextureCache::setReplacementDefaultShift(ReplacementShift shift) {
        std::unique_lock lock(textureMapMutex);
        if (!textureMap.replacementMap.fileSystemIsDirectory) {
            return;
        }

        ReplacementDatabase &replacementDb = textureMap.replacementMap.directoryDatabase;
        replacementDb.config.defaultShift = shift;
        replacementDb.resolveShifts(textureMap.replacementMap.fileSystemResolvedPaths[0]);
        reloadReplacements(false);
    }

    void TextureCache::setReplacementDefaultOperation(ReplacementOperation operation) {
        std::unique_lock lock(textureMapMutex);
        if (!textureMap.replacementMap.fileSystemIsDirectory) {
            return;
        }

        ReplacementDatabase &replacementDb = textureMap.replacementMap.directoryDatabase;
        replacementDb.config.defaultOperation = operation;
        replacementDb.resolveOperations(textureMap.replacementMap.fileSystemResolvedPaths[0]);
        reloadReplacements(false);
    }

    void setReplacementTextureOperationOrShift(TextureCache &cache, uint64_t hash, ReplacementTexture texture) {
        ReplacementDatabase &replacementDb = cache.textureMap.replacementMap.directoryDatabase;
        replacementDb.addReplacement(texture);
        auto it = cache.textureMap.replacementMap.fileSystemResolvedPaths[0].find(hash);
        if (it != cache.textureMap.replacementMap.fileSystemResolvedPaths[0].end()) {
            it->second.originalOperation = texture.operation;
            it->second.originalShift = texture.shift;
            it->second.resolvedOperation = replacementDb.resolveOperation(it->second.relativePath, texture.operation);
            it->second.resolvedShift = replacementDb.resolveShift(it->second.relativePath, texture.shift);

            // Since we don't have exact mappings from the database hashes to the cache hashes, we run some hash checks
            // on all currently loaded textures to check for replacements.
            cache.reloadReplacements(false, hash);
        }
    }

    void TextureCache::setReplacementShift(uint64_t hash, ReplacementShift shift) {
        std::unique_lock lock(textureMapMutex);
        if (!textureMap.replacementMap.fileSystemIsDirectory) {
            return;
        }

        ReplacementDatabase &replacementDb = textureMap.replacementMap.directoryDatabase;
        ReplacementTexture replacement = replacementDb.getReplacement(hash);
        if (!replacement.isEmpty()) {
            replacement.shift = shift;
            setReplacementTextureOperationOrShift(*this, hash, replacement);
        }
    }

    void TextureCache::setReplacementOperation(uint64_t hash, ReplacementOperation operation) {
        std::unique_lock lock(textureMapMutex);
        if (!textureMap.replacementMap.fileSystemIsDirectory) {
            return;
        }

        ReplacementDatabase &replacementDb = textureMap.replacementMap.directoryDatabase;
        ReplacementTexture replacement = replacementDb.getReplacement(hash);
        if (!replacement.isEmpty()) {
            replacement.operation = operation;
            setReplacementTextureOperationOrShift(*this, hash, replacement);
        }
    }

    void TextureCache::removeUnusedEntriesFromDatabase() {
        std::unique_lock lock(textureMapMutex);
        if (!textureMap.replacementMap.fileSystemIsDirectory) {
            return;
        }

        textureMap.replacementMap.removeUnusedEntriesFromDatabase();
    }

    Texture *TextureCache::getTexture(uint32_t textureIndex) {
        std::unique_lock lock(textureMapMutex);
        return textureMap.get(textureIndex);
    }

    bool TextureCache::evict(uint64_t submissionFrame, std::vector<uint64_t> &evictedHashes) {
        std::unique_lock lock(textureMapMutex);
        return textureMap.evict(submissionFrame, evictedHashes);
    }

    void TextureCache::incrementLock() {
        std::unique_lock lock(textureMapMutex);
        lockCounter++;
    }

    void TextureCache::decrementLock() {
        std::unique_lock lock(textureMapMutex);
        lockCounter--;

        if (lockCounter == 0) {
            // Delete evicted textures from texture map.
            for (Texture *texture : textureMap.evictedTextures) {
                delete texture;
            }

            textureMap.evictedTextures.clear();
        }
    }

    void TextureCache::waitForAllStreamThreads(bool clearQueueImmediately) {
        if (clearQueueImmediately) {
            std::unique_lock<std::mutex> queueLock(streamDescStackMutex);
            streamDescStack = std::stack<StreamDescription>();
        }

        bool keepWaiting = false;
        do {
            streamDescStackMutex.lock();
            keepWaiting = (streamDescStackActiveCount > 0);
            streamDescStackMutex.unlock();

            if (keepWaiting) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        } while (keepWaiting);
    }

    void TextureCache::resetStreamPerformanceCounters() {
        std::unique_lock lock(streamPerformanceMutex);
        streamLoadTimeTotal = 0;
        streamLoadCount = 0;
    }

    void TextureCache::addStreamLoadTime(uint64_t streamLoadTime) {
        std::unique_lock lock(streamPerformanceMutex);
        streamLoadTimeTotal += streamLoadTime;
        streamLoadCount++;
    }

    uint64_t TextureCache::getAverageStreamLoadTime() {
        std::unique_lock lock(streamPerformanceMutex);
        if (streamLoadTimeTotal > 0) {
            return streamLoadTimeTotal / streamLoadCount;
        }
        else {
            return 0;
        }
    }

    void TextureCache::setReplacementPoolMaxSize(uint64_t maxSize) {
        std::unique_lock lock(textureMapMutex);
        textureMap.replacementMap.maxTexturePoolSize = maxSize;
    }

    void TextureCache::getReplacementPoolStats(uint64_t &usedSize, uint64_t &cachedSize, uint64_t &maxSize) {
        std::unique_lock lock(textureMapMutex);
        usedSize = textureMap.replacementMap.usedTexturePoolSize;
        cachedSize = textureMap.replacementMap.cachedTexturePoolSize;
        maxSize = textureMap.replacementMap.maxTexturePoolSize;
    }
};
