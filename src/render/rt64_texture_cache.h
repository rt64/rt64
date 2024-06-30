//
// RT64
//

#pragma once

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

#include <json/json.hpp>

#include "common/rt64_replacement_database.h"
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
        uint32_t width;
        uint32_t height;
        uint32_t tlut;
        LoadTile loadTile;
        std::vector<uint8_t> bytesTMEM;
        bool decodeTMEM;
    };

    typedef std::pair<uint32_t, uint64_t> AccessPair;
    typedef std::list<AccessPair> AccessList;

    struct ReplacementMap {
        ReplacementDatabase db;
        std::vector<Texture *> loadedTextures;
        std::vector<Texture *> evictedTextures;
        std::unordered_map<uint64_t, uint32_t> pathHashToLoadMap;
        std::unordered_map<uint64_t, std::string> autoPathMap;
        std::filesystem::path directoryPath;

        ReplacementMap();
        ~ReplacementMap();
        void clear();
        bool readDatabase(std::istream &stream);
        bool saveDatabase(std::ostream &stream);
        void resolveAutoPaths();
        void removeUnusedEntriesFromDatabase();
        std::string getRelativePathFromHash(uint64_t tmemHash) const;
        Texture *getFromRelativePath(const std::string &relativePath) const;
        Texture *loadFromBytes(RenderWorker *worker, uint64_t hash, const std::string &relativePath, const std::vector<uint8_t> &fileBytes, std::unique_ptr<RenderBuffer> &dstUploadResource, RenderPool *resourcePool = nullptr, uint32_t minMipWidth = 0, uint32_t minMipHeight = 0);
        uint64_t hashFromRelativePath(const std::string &relativePath) const;
    };

    struct ReplacementCheck {
        uint64_t textureHash = 0;
        uint64_t databaseHash = 0;
        uint32_t minMipWidth = 0;
        uint32_t minMipHeight = 0;
    };

    struct HashTexturePair {
        uint64_t hash = 0;
        Texture *texture = nullptr;
    };

    struct TextureMap {
        std::unordered_map<uint64_t, uint32_t> hashMap;
        std::vector<Texture *> textures;
        std::vector<Texture *> textureReplacements;
        std::vector<interop::float2> textureScales;
        std::vector<uint64_t> hashes;
        std::vector<uint32_t> freeSpaces;
        std::vector<uint32_t> versions;
        std::vector<uint64_t> creationFrames;
        uint32_t globalVersion;
        AccessList accessList;
        std::vector<AccessList::iterator> listIterators;
        std::vector<Texture *> evictedTextures;
        ReplacementMap replacementMap;
        bool replacementMapEnabled;

        TextureMap();
        ~TextureMap();
        void clearReplacements();
        void add(uint64_t hash, uint64_t creationFrame, Texture *texture);
        void replace(uint64_t hash, Texture *texture);
        bool use(uint64_t hash, uint64_t submissionFrame, uint32_t &textureIndex, interop::float2 &textureScale, bool &textureReplaced, bool &hasMipmaps);
        bool evict(uint64_t submissionFrame, std::vector<uint64_t> &evictedHashes);
        void incrementLock();
        void decrementLock();
        Texture *get(uint32_t index) const;
        size_t getMaxIndex() const;
    };

    struct TextureCache {
        const ShaderLibrary *shaderLibrary;
        std::vector<TextureUpload> uploadQueue;
        std::vector<ReplacementCheck> replacementQueue;
        std::vector<std::unique_ptr<RenderBuffer>> tmemUploadResources;
        std::vector<std::unique_ptr<RenderBuffer>> replacementUploadResources;
        std::vector<std::unique_ptr<TextureDecodeDescriptorSet>> descriptorSets;
        std::mutex uploadQueueMutex;
        std::condition_variable uploadQueueChanged;
        std::condition_variable uploadQueueFinished;
        std::thread *uploadThread;
        std::atomic<bool> uploadThreadRunning;
        TextureMap textureMap;
        std::mutex textureMapMutex;
        RenderWorker *worker;
        std::unique_ptr<RenderPool> threadResourcePool;
        uint32_t lockCounter;
        bool developerMode;

        TextureCache(RenderWorker *worker, const ShaderLibrary *shaderLibrary, bool developerMode);
        ~TextureCache();
        void uploadThreadLoop();
        void queueGPUUploadTMEM(uint64_t hash, uint64_t creationFrame, const uint8_t *bytes, int bytesCount, int width, int height, uint32_t tlut, const LoadTile &loadTile, bool decodeTMEM);
        void waitForGPUUploads();
        bool useTexture(uint64_t hash, uint64_t submissionFrame, uint32_t &textureIndex, interop::float2 &textureScale, bool &textureReplaced, bool &hasMipmaps);
        bool useTexture(uint64_t hash, uint64_t submissionFrame, uint32_t &textureIndex);
        bool addReplacement(uint64_t hash, const std::string &relativePath);
        bool loadReplacementDirectory(const std::filesystem::path &directoryPath);
        bool saveReplacementDatabase();
        void removeUnusedEntriesFromDatabase();
        bool evict(uint64_t submissionFrame, std::vector<uint64_t> &evictedHashes);
        void incrementLock();
        void decrementLock();
        Texture *getTexture(uint32_t textureIndex);
        static void setRGBA32(Texture *dstTexture, RenderWorker *worker, const uint8_t *bytes, size_t byteCount, uint32_t width, uint32_t height, uint32_t rowPitch, std::unique_ptr<RenderBuffer> &dstUploadResource, RenderPool *uploadResourcePool = nullptr);
        static bool setDDS(Texture *dstTexture, RenderWorker *worker, const uint8_t *bytes, size_t byteCount, std::unique_ptr<RenderBuffer> &dstUploadResource, RenderPool *uploadResourcePool = nullptr, uint32_t minMipWidth = 0, uint32_t minMipHeight = 0);
        static bool loadBytesFromPath(const std::filesystem::path &path, std::vector<uint8_t> &bytes);
    };
};