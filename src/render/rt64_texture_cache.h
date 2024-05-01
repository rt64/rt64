//
// RT64
//

#pragma once

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <thread>
#include <unordered_map>

#include "hle/rt64_draw_call.h"

#include "rt64_descriptor_sets.h"
#include "rt64_render_worker.h"
#include "rt64_shader_library.h"
#include "rt64_texture.h"

namespace interop {
    struct alignas(16) TextureDecodeCB {
        uint2 Resolution;
        uint fmt;
        uint siz;
        uint address;
        uint stride;
        uint tlut;
        uint palette;
    };
};

namespace RT64 {
    struct TextureUpload {
        uint64_t hash;
        uint64_t creationFrame;
        int width;
        int height;
        uint32_t tlut;
        LoadTile loadTile;
        std::vector<uint8_t> bytesTMEM;
    };

    struct TextureMap {
        typedef std::pair<uint32_t, uint64_t> AccessPair;
        typedef std::list<AccessPair> AccessList;

        std::unordered_map<uint64_t, uint32_t> hashMap;
        std::vector<const Texture *> textures;
        std::vector<uint64_t> hashes;
        std::vector<uint32_t> freeSpaces;
        std::vector<uint32_t> versions;
        std::vector<uint64_t> creationFrames;
        uint32_t globalVersion;
        AccessList accessList;
        std::vector<AccessList::iterator> listIterators;
        std::vector<const Texture *> evictedTextures;
        uint32_t lockCounter;

        TextureMap();
        ~TextureMap();
        void add(uint64_t hash, uint64_t creationFrame, const Texture *texture);
        bool use(uint64_t hash, uint64_t submissionFrame, uint32_t &textureIndex);
        bool evict(uint64_t submissionFrame, std::vector<uint64_t> &evictedHashes);
        void incrementLock();
        void decrementLock();
        const Texture *get(uint32_t index) const;
        size_t getMaxIndex() const;
    };

    struct TextureCache {
        const ShaderLibrary *shaderLibrary;
        std::vector<TextureUpload> uploadQueue;
        std::vector<std::unique_ptr<RenderBuffer>> uploadResources;
        std::vector<std::unique_ptr<TextureDecodeDescriptorSet>> descriptorSets;
        std::mutex uploadQueueMutex;
        std::condition_variable uploadQueueChanged;
        std::condition_variable uploadQueueFinished;
        std::thread *uploadThread;
        std::atomic<bool> uploadThreadRunning;
        TextureMap textureMap;
        std::mutex textureMapMutex;
        RenderWorker *worker;
        std::unique_ptr<RenderPool> uploadResourcePool;
        bool developerMode;

        TextureCache(RenderWorker *worker, const ShaderLibrary *shaderLibrary, bool developerMode);
        ~TextureCache();
        void uploadThreadLoop();
        void queueGPUUploadTMEM(uint64_t hash, uint64_t creationFrame, const uint8_t *bytes, int bytesCount, int width, int height, uint32_t tlut, const LoadTile &loadTile);
        void waitForGPUUploads();
        bool useTexture(uint64_t hash, uint64_t submissionFrame, uint32_t &textureIndex);
        bool evict(uint64_t submissionFrame, std::vector<uint64_t> &evictedHashes);
        void incrementLock();
        void decrementLock();
        const Texture *getTexture(uint32_t textureIndex);
        static void setRGBA32(Texture *dstTexture, RenderWorker *worker, const void *bytes, int byteCount, int width, int height, int rowPitch, std::unique_ptr<RenderBuffer> &dstUploadResource, RenderPool *uploadResourcePool);
    };
};