//
// RT64
//

#pragma once

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

#include "rt64_raster_shader.h"

namespace RT64 {
    struct RasterShaderCache {
        struct OfflineList {
            struct Entry {
                ShaderDescription shaderDesc;
                std::vector<uint8_t> vsDxilBytes;
                std::vector<uint8_t> psDxilBytes;
            };

            std::list<Entry> entries;
            std::list<Entry>::iterator entryIterator;

            OfflineList();
            bool load(std::istream &stream);
            void reset();
            void step(Entry &entry);
            bool atEnd() const;
        };

        struct OfflineDumper {
            std::ofstream dumpStream;

            bool startDumping(const std::filesystem::path &path);
            bool stepDumping(const ShaderDescription &shaderDesc, const std::vector<uint8_t> &vsDxilBytes, const std::vector<uint8_t> &psDxilBytes);
            bool stopDumping();
            bool isDumping() const;
        };

        struct CompilationThread {
            RasterShaderCache *shaderCache;
            std::unique_ptr<std::thread> thread;
            std::atomic<bool> threadRunning;

            CompilationThread(RasterShaderCache *shaderCache);
            ~CompilationThread();
            void loop();
        };

        RenderDevice *device;
        std::unique_ptr<RasterShaderUber> shaderUber;
        std::mutex submissionMutex;
        std::queue<ShaderDescription> descQueue;
        std::mutex descQueueMutex;
        int32_t descQueueActiveCount = 0;
        std::condition_variable descQueueChanged;
        std::unordered_map<uint64_t, bool> shaderHashes;
        std::unordered_map<uint64_t, std::unique_ptr<RasterShader>> GPUShaders;
        std::mutex GPUShadersMutex;
        std::list<std::unique_ptr<CompilationThread>> compilationThreads;
        uint32_t threadCount;
        RenderShaderFormat shaderFormat;
        std::unique_ptr<ShaderCompiler> shaderCompiler;
        RenderMultisampling multisampling;
        OfflineList offlineList;
        OfflineDumper offlineDumper;
        std::mutex offlineDumperMutex;
        bool usesHDR = false;
        
        RasterShaderCache(uint32_t threadCount);
        ~RasterShaderCache();
        void setup(RenderDevice *device, RenderShaderFormat shaderFormat, const ShaderLibrary *shaderLibrary, const RenderMultisampling &multisampling);
        void submit(const ShaderDescription &desc);
        void waitForAll();
        void destroyAll();
        RasterShader *getGPUShader(const ShaderDescription &desc);
        RasterShaderUber *getGPUShaderUber() const;
        bool isOfflineDumperActive();
        bool startOfflineDumper(const std::filesystem::path &path);
        bool stopOfflineDumper();
        bool loadOfflineList(std::istream &stream);
        void resetOfflineList();
        uint32_t shaderCount();
    };
};